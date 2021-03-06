//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2020 MuseScore BVBA and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================
#include "globalmodule.h"

#include <QTimer>

#include "modularity/ioc.h"
#include "internal/globalconfiguration.h"

#include "log.h"
#include "thirdparty/haw_logger/logger/logdefdest.h"
#include "version.h"

#include "internal/interactive.h"
#include "invoker.h"

#include "runtime.h"
#include "async/processevents.h"

#include "settings.h"

using namespace mu::framework;

static std::shared_ptr<GlobalConfiguration> s_globalConf = std::make_shared<GlobalConfiguration>();

static Invoker s_asyncInvoker;

std::string GlobalModule::moduleName() const
{
    return "global";
}

void GlobalModule::registerExports()
{
    ioc()->registerExport<IGlobalConfiguration>(moduleName(), s_globalConf);
    ioc()->registerExport<IInteractive>(moduleName(), new Interactive());
}

void GlobalModule::onInit()
{
    //! NOTE: settings must be inited before initialization of any module
    //! because modules can use settings at the moment of their initialization
    settings()->load();

    //! --- Setup logger ---
    using namespace haw::logger;
    Logger* logger = Logger::instance();
    logger->clearDests();

    //! Console
    logger->addDest(new ConsoleLogDest(LogLayout("${time} | ${type|5} | ${thread} | ${tag|10} | ${message}")));

    //! File, this creates a file named "data/logs/MuseScore_yyMMdd.log"
    std::string logsPath = s_globalConf->logsPath().c_str();
    LOGI() << "logs path: " << logsPath;
    logger->addDest(new FileLogDest(logsPath, "MuseScore", "log",
                                    LogLayout("${datetime} | ${type|5} | ${thread} | ${tag|10} | ${message}")));

#ifndef NDEBUG
    logger->setLevel(haw::logger::Debug);
#else
    logger->setLevel(haw::logger::Normal);
#endif

    LOGI() << "=== Started MuseScore " << framework::Version::fullVersion() << " ===";

    //! --- Setup profiler ---
    using namespace haw::profiler;
    struct MyPrinter : public Profiler::Printer
    {
        void printDebug(const std::string& str) override { LOG_STREAM(Logger::DEBUG, "Profiler") << str; }
        void printInfo(const std::string& str) override { LOG_STREAM(Logger::INFO, "Profiler") << str; }
    };

    Profiler::Options profOpt;
    profOpt.stepTimeEnabled = true;
    profOpt.funcsTimeEnabled = true;
    profOpt.funcsTraceEnabled = false;
    profOpt.funcsMaxThreadCount = 100;
    profOpt.dataTopCount = 150;

    Profiler* profiler = Profiler::instance();
    profiler->setup(profOpt, new MyPrinter());

    //! --- Setup Invoker ---

    Invoker::setup();

    mu::async::onMainThreadInvoke([](const std::function<void()>& f) {
        s_asyncInvoker.invoke(f);
    });
}

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_linux.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "components/password_manager/core/browser/password_manager_switches.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/ozone/public/ozone_platform.h"
#if BUILDFLAG(USE_DBUS)
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#endif  // BUILDFLAG(USE_DBUS)
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/call_stacks/stack_sampling_recorder.h"
#endif

#if BUILDFLAG(USE_DBUS) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/dbus_memory_pressure_evaluator_linux.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "base/linux_util.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#endif

ChromeBrowserMainPartsLinux::ChromeBrowserMainPartsLinux(
    bool is_integration_test,
    StartupData* startup_data)
    : ChromeBrowserMainPartsPosix(is_integration_test, startup_data) {}

ChromeBrowserMainPartsLinux::~ChromeBrowserMainPartsLinux() = default;

void ChromeBrowserMainPartsLinux::PostCreateMainMessageLoop() {
#if BUILDFLAG(IS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          metrics::kRecordStackSamplingDataSwitch)) {
    stack_sampling_recorder_ =
        base::MakeRefCounted<metrics::StackSamplingRecorder>();
    stack_sampling_recorder_->Start();
  }
  // Don't initialize DBus here. Bluetooth DBusManager initialization depends on
  // FeatureList, and is done elsewhere.
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(USE_DBUS)
  bluez::BluezDBusManager::Initialize(
      dbus_thread_linux::GetSharedSystemBus().get());
#endif  // BUILDFLAG(USE_DBUS)
#endif  // !BUILDFLAG(IS_CHROMEOS)

  ChromeBrowserMainPartsPosix::PostCreateMainMessageLoop();
}

#if BUILDFLAG(IS_LINUX)
void ChromeBrowserMainPartsLinux::PostMainMessageLoopRun() {
  ChromeBrowserMainPartsPosix::PostMainMessageLoopRun();
  ui::OzonePlatform::GetInstance()->PostMainMessageLoopRun();
}
#endif

void ChromeBrowserMainPartsLinux::PreProfileInit() {
#if !BUILDFLAG(IS_CHROMEOS)
  // Needs to be called after we have chrome::DIR_USER_DATA and
  // g_browser_process.  This happens in PreCreateThreads.
  // base::GetLinuxDistro() will initialize its value if needed.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(base::IgnoreResult(&base::GetLinuxDistro)));
#endif

  ChromeBrowserMainPartsPosix::PreProfileInit();
}

#if BUILDFLAG(USE_DBUS) && !BUILDFLAG(IS_CHROMEOS)
void ChromeBrowserMainPartsLinux::PostBrowserStart() {
  auto* monitor = memory_pressure::MultiSourceMemoryPressureMonitor::Get();
  if (monitor &&
      base::FeatureList::IsEnabled(features::kLinuxLowMemoryMonitor)) {
    monitor->SetSystemEvaluator(
        std::make_unique<DbusMemoryPressureEvaluatorLinux>(
            monitor->CreateVoter()));
  }
  ChromeBrowserMainPartsPosix::PostBrowserStart();
}
#endif  // BUILDFLAG(USE_DBUS) && !BUILDFLAG(IS_CHROMEOS)

void ChromeBrowserMainPartsLinux::PostDestroyThreads() {
#if BUILDFLAG(IS_CHROMEOS)
  // No-op; per PostBrowserStart() comment, this is done elsewhere.
#else
  bluez::BluezDBusManager::Shutdown();
#endif  // BUILDFLAG(IS_CHROMEOS)

  ChromeBrowserMainPartsPosix::PostDestroyThreads();
}

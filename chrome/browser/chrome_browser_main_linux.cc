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
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "components/password_manager/core/browser/password_manager_switches.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/bluez_dbus_thread_manager.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/tast_support/stack_sampling_recorder.h"
#include "chrome/installer/util/google_update_settings.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/dbus/lacros_dbus_thread_manager.h"
#endif

#if defined(USE_DBUS) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/dbus_memory_pressure_evaluator_linux.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/linux_util.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "components/os_crypt/sync/key_storage_config_linux.h"
#include "components/os_crypt/sync/os_crypt.h"
#endif

ChromeBrowserMainPartsLinux::ChromeBrowserMainPartsLinux(
    bool is_integration_test,
    StartupData* startup_data)
    : ChromeBrowserMainPartsPosix(is_integration_test, startup_data) {}

ChromeBrowserMainPartsLinux::~ChromeBrowserMainPartsLinux() {
}

void ChromeBrowserMainPartsLinux::PostCreateMainMessageLoop() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_CHROMEOS)
  if (command_line->HasSwitch(
          chromeos::tast_support::kRecordStackSamplingDataSwitch)) {
    stack_sampling_recorder_ =
        base::MakeRefCounted<chromeos::tast_support::StackSamplingRecorder>();
    stack_sampling_recorder_->Start();
  }
  // Don't initialize DBus here. Ash and Lacros Bluetooth DBusManager
  // initialization depend on FeatureList, and is done elsewhere.
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS)
  bluez::BluezDBusManager::Initialize(nullptr /* system_bus */);

  // Set up crypt config. This needs to be done before anything starts the
  // network service, as the raw encryption key needs to be shared with the
  // network service for encrypted cookie storage.
  // Chrome OS does not need a crypt config as its user data directories are
  // already encrypted and none of the true encryption backends used by desktop
  // Linux are available on Chrome OS anyway.
  std::unique_ptr<os_crypt::Config> config =
      std::make_unique<os_crypt::Config>();
  // Forward to os_crypt the flag to use a specific password store.
  config->store =
      command_line->GetSwitchValueASCII(password_manager::kPasswordStore);
  // Forward the product name
  config->product_name = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  // OSCrypt can be disabled in a special settings file.
  config->should_use_preference =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          password_manager::kEnableEncryptionSelection);
  chrome::GetDefaultUserDataDirectory(&config->user_data_path);
  OSCrypt::SetConfig(std::move(config));
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
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Needs to be called after we have chrome::DIR_USER_DATA and
  // g_browser_process.  This happens in PreCreateThreads.
  // base::GetLinuxDistro() will initialize its value if needed.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(base::IgnoreResult(&base::GetLinuxDistro)));
#endif

  ChromeBrowserMainPartsPosix::PreProfileInit();
}

#if defined(USE_DBUS) && !BUILDFLAG(IS_CHROMEOS)
void ChromeBrowserMainPartsLinux::PostBrowserStart() {
  // static_cast is safe because this is the only implementation of
  // MemoryPressureMonitor.
  auto* monitor =
      static_cast<memory_pressure::MultiSourceMemoryPressureMonitor*>(
          base::MemoryPressureMonitor::Get());
  if (monitor &&
      base::FeatureList::IsEnabled(features::kLinuxLowMemoryMonitor)) {
    monitor->SetSystemEvaluator(
        std::make_unique<DbusMemoryPressureEvaluatorLinux>(
            monitor->CreateVoter()));
  }
  ChromeBrowserMainPartsPosix::PostBrowserStart();
}
#endif  // (defined(USE_DBUS) && !BUILDFLAG(IS_CHROMEOS))

void ChromeBrowserMainPartsLinux::PostDestroyThreads() {
#if BUILDFLAG(IS_CHROMEOS)
  // No-op; per PostBrowserStart() comment, this is done elsewhere.
#else
  bluez::BluezDBusManager::Shutdown();
  bluez::BluezDBusThreadManager::Shutdown();
#endif  // BUILDFLAG(IS_CHROMEOS)

  ChromeBrowserMainPartsPosix::PostDestroyThreads();
}

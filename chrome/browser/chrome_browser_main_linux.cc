// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_linux.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/chromium_strings.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/content/app/crashpad.h"
#include "components/metrics/metrics_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/bluez_dbus_thread_manager.h"
#include "media/audio/audio_manager.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "base/linux_util.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "components/os_crypt/key_storage_config_linux.h"
#include "components/os_crypt/os_crypt.h"
#include "content/public/browser/browser_thread.h"
#endif

ChromeBrowserMainPartsLinux::ChromeBrowserMainPartsLinux(
    const content::MainFunctionParams& parameters,
    StartupData* startup_data)
    : ChromeBrowserMainPartsPosix(parameters, startup_data) {}

ChromeBrowserMainPartsLinux::~ChromeBrowserMainPartsLinux() {
}

void ChromeBrowserMainPartsLinux::PreProfileInit() {
#if !defined(OS_CHROMEOS)
  // Needs to be called after we have chrome::DIR_USER_DATA and
  // g_browser_process.  This happens in PreCreateThreads.
  // base::GetLinuxDistro() will initialize its value if needed.
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(base::IgnoreResult(&base::GetLinuxDistro)));
#endif

  media::AudioManager::SetGlobalAppName(
      l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME));

#if !defined(OS_CHROMEOS)
  // Set up crypt config. This should be kept in sync with the OSCrypt parts of
  // SystemNetworkContextManager::OnNetworkServiceCreated.
  std::unique_ptr<os_crypt::Config> config(new os_crypt::Config());
  // Forward to os_crypt the flag to use a specific password store.
  config->store =
      parsed_command_line().GetSwitchValueASCII(switches::kPasswordStore);
  // Forward the product name
  config->product_name = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  // OSCrypt may target keyring, which requires calls from the main thread.
  config->main_thread_runner =
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI});
  // OSCrypt can be disabled in a special settings file.
  config->should_use_preference =
      parsed_command_line().HasSwitch(switches::kEnableEncryptionSelection);
  chrome::GetDefaultUserDataDirectory(&config->user_data_path);
  OSCrypt::SetConfig(std::move(config));
#endif

  ChromeBrowserMainPartsPosix::PreProfileInit();
}

void ChromeBrowserMainPartsLinux::PostProfileInit() {
  ChromeBrowserMainPartsPosix::PostProfileInit();

  bool enabled = (crash_reporter::IsCrashpadEnabled() &&
                  crash_reporter::GetUploadsEnabled()) ||
                 breakpad::IsCrashReporterEnabled();
  g_browser_process->metrics_service()->RecordBreakpadRegistration(enabled);
}

void ChromeBrowserMainPartsLinux::PostMainMessageLoopStart() {
#if !defined(OS_CHROMEOS)
  bluez::BluezDBusManager::Initialize(nullptr /* system_bus */);
#endif

  ChromeBrowserMainPartsPosix::PostMainMessageLoopStart();
}

void ChromeBrowserMainPartsLinux::PostDestroyThreads() {
#if !defined(OS_CHROMEOS)
  bluez::BluezDBusManager::Shutdown();
  bluez::BluezDBusThreadManager::Shutdown();
#endif

  ChromeBrowserMainPartsPosix::PostDestroyThreads();
}

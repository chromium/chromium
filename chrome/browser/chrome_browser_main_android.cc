// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_android.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/task/current_thread.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/mojo/chrome_interface_registrar_android.h"
#include "chrome/browser/android/preferences/clipboard_android.h"
#include "chrome/browser/android/seccomp_support_detector.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/webauthn/android/cable_module_android.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/child_process_crash_observer_android.h"
#include "components/metrics/stability_metrics_helper.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/main_function_params.h"
#include "device/fido/features.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_paths.h"

ChromeBrowserMainPartsAndroid::ChromeBrowserMainPartsAndroid(
    const content::MainFunctionParams& parameters,
    StartupData* startup_data)
    : ChromeBrowserMainParts(parameters, startup_data) {}

ChromeBrowserMainPartsAndroid::~ChromeBrowserMainPartsAndroid() {
}

int ChromeBrowserMainPartsAndroid::PreCreateThreads() {
  TRACE_EVENT0("startup", "ChromeBrowserMainPartsAndroid::PreCreateThreads");

  int result_code = ChromeBrowserMainParts::PreCreateThreads();

  // The ChildExitObserver needs to be created before any child process is
  // created because it needs to be notified during process creation.
  crash_reporter::ChildExitObserver::Create();
  crash_reporter::ChildExitObserver::GetInstance()->RegisterClient(
      std::make_unique<crash_reporter::ChildProcessCrashObserver>());

  return result_code;
}

void ChromeBrowserMainPartsAndroid::PostProfileInit() {
  ChromeBrowserMainParts::PostProfileInit();

  // Idempotent.  Needs to be called once on startup.  If
  // InitializeClipboardAndroidFromLocalState() is called multiple times (e.g.,
  // once per profile load), that's okay; the additional calls don't change
  // anything.
  android::InitClipboardAndroidFromLocalState(g_browser_process->local_state());

  // Start watching the preferences that need to be backed up backup using
  // Android backup, so that we create a new backup if they change.
  backup_watcher_.reset(new android::ChromeBackupWatcher(profile()));

  // The GCM driver can be used at this point because the primary profile has
  // been created. Register non-profile-specific things that use GCM so that no
  // messages can be processed (and dropped) because the handler wasn't
  // installed in time.
  if (base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
    webauthn::authenticator::RegisterForCloudMessages();
  }
}

int ChromeBrowserMainPartsAndroid::PreEarlyInitialization() {
  TRACE_EVENT0("startup",
               "ChromeBrowserMainPartsAndroid::PreEarlyInitialization");
  content::Compositor::Initialize();

  CHECK(base::CurrentThread::IsSet());

  return ChromeBrowserMainParts::PreEarlyInitialization();
}

void ChromeBrowserMainPartsAndroid::PostEarlyInitialization() {
  profile_manager_android_.reset(new ProfileManagerAndroid());
  g_browser_process->profile_manager()->AddObserver(
      profile_manager_android_.get());
  ChromeBrowserMainParts::PostEarlyInitialization();
}

void ChromeBrowserMainPartsAndroid::PostBrowserStart() {
  ChromeBrowserMainParts::PostBrowserStart();

  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReportSeccompSupport), base::TimeDelta::FromMinutes(1));

  RegisterChromeJavaMojoInterfaces();
}

void ChromeBrowserMainPartsAndroid::ShowMissingLocaleMessageBox() {
  NOTREACHED();
}

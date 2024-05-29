// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/mojo/chrome_interface_registrar_android.h"
#include "chrome/browser/android/preferences/clipboard_android.h"
#include "chrome/browser/android/seccomp_support_detector.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_saver/data_saver.h"
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

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeBackupWatcher_jni.h"

ChromeBrowserMainPartsAndroid::ChromeBrowserMainPartsAndroid(
    bool is_integration_test,
    StartupData* startup_data)
    : ChromeBrowserMainParts(is_integration_test, startup_data) {}

ChromeBrowserMainPartsAndroid::~ChromeBrowserMainPartsAndroid() {
}

int ChromeBrowserMainPartsAndroid::PreCreateThreads() {
  TRACE_EVENT0("startup", "ChromeBrowserMainPartsAndroid::PreCreateThreads");

  int result_code = ChromeBrowserMainParts::PreCreateThreads();

  // The ChildExitObserver needs to be created before any child process is
  // created because it needs to be notified during process creation.
  child_exit_observer_ = std::make_unique<crash_reporter::ChildExitObserver>();
  child_exit_observer_->RegisterClient(
      std::make_unique<crash_reporter::ChildProcessCrashObserver>());

  return result_code;
}

void ChromeBrowserMainPartsAndroid::PostProfileInit(Profile* profile,
                                                    bool is_initial_profile) {
  DCHECK(is_initial_profile);  // No multiprofile on Android, only the initial
                               // call should happen.

  // Get the OS Data Saver setting. This will be needed later on, so we want to
  // fetch this setting as soon as possible to avoid blocking on it.
  data_saver::FetchDataSaverOSSettingAsynchronously();

  ChromeBrowserMainParts::PostProfileInit(profile, is_initial_profile);

  // Idempotent.  Needs to be called once on startup.  If
  // InitializeClipboardAndroidFromLocalState() is called multiple times (e.g.,
  // once per profile load), that's okay; the additional calls don't change
  // anything.
  android::InitClipboardAndroidFromLocalState(g_browser_process->local_state());

  // Start watching the preferences that need to be backed up backup using
  // Android backup, so that we create a new backup if they change.
  base::android::ScopedJavaGlobalRef<jobject> watcher;
  watcher.Reset(android::Java_ChromeBackupWatcher_Constructor(
      base::android::AttachCurrentThread()));
  backup_watcher_runner_.ReplaceClosure(
      base::BindOnce(&android::Java_ChromeBackupWatcher_destroy,
                     base::android::AttachCurrentThread(), watcher));

  // The GCM driver can be used at this point because the primary profile has
  // been created. Register non-profile-specific things that use GCM so that no
  // messages can be processed (and dropped) because the handler wasn't
  // installed in time.
  webauthn::authenticator::RegisterForCloudMessages();
}

int ChromeBrowserMainPartsAndroid::PreEarlyInitialization() {
  TRACE_EVENT0("startup",
               "ChromeBrowserMainPartsAndroid::PreEarlyInitialization");
  content::Compositor::Initialize();

  CHECK(base::CurrentThread::IsSet());

  return ChromeBrowserMainParts::PreEarlyInitialization();
}

void ChromeBrowserMainPartsAndroid::PostBrowserStart() {
  ChromeBrowserMainParts::PostBrowserStart();

  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReportSeccompSupport), base::Minutes(1));

  RegisterChromeJavaMojoInterfaces();
}

void ChromeBrowserMainPartsAndroid::ShowMissingLocaleMessageBox() {
  NOTREACHED_IN_MIGRATION();
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_android.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/mojo/chrome_interface_registrar_android.h"
#include "chrome/browser/android/preferences/clipboard_android.h"
#include "chrome/browser/android/seccomp_support_detector.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/descriptors_android.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/child_process_crash_observer_android.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/main_function_params.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_paths.h"

ChromeBrowserMainPartsAndroid::ChromeBrowserMainPartsAndroid(
    const content::MainFunctionParams& parameters,
    ChromeFeatureListCreator* chrome_feature_list_creator)
    : ChromeBrowserMainParts(parameters,
                             chrome_feature_list_creator) {}

ChromeBrowserMainPartsAndroid::~ChromeBrowserMainPartsAndroid() {
}

int ChromeBrowserMainPartsAndroid::PreCreateThreads() {
  TRACE_EVENT0("startup", "ChromeBrowserMainPartsAndroid::PreCreateThreads")

  int result_code = ChromeBrowserMainParts::PreCreateThreads();

  // The ChildProcessCrashObserver must be registered before any child
  // process is created (as it needs to be notified during child
  // process creation). Such processes are created on the
  // PROCESS_LAUNCHER thread, and so the observer is initialized and
  // the manager registered before that thread is created.
  crash_reporter::ChildExitObserver::Create();

#if defined(GOOGLE_CHROME_BUILD)
  // TODO(jcivelli): we should not initialize the crash-reporter when it was not
  // enabled. Right now if it is disabled we still generate the minidumps but we
  // do not upload them.
  bool breakpad_enabled = true;
#else
  bool breakpad_enabled = false;
#endif

  // Allow Breakpad to be enabled in Chromium builds for testing purposes.
  if (!breakpad_enabled)
    breakpad_enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableCrashReporterForTesting);

  if (breakpad_enabled) {
    base::FilePath crash_dump_dir;
    base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dump_dir);
    crash_reporter::ChildExitObserver::GetInstance()->RegisterClient(
        std::make_unique<crash_reporter::ChildProcessCrashObserver>(
            crash_dump_dir, kAndroidMinidumpDescriptor));
  }

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
}

int ChromeBrowserMainPartsAndroid::PreEarlyInitialization() {
  TRACE_EVENT0("startup",
    "ChromeBrowserMainPartsAndroid::PreEarlyInitialization")
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());

  content::Compositor::Initialize();

  // Chrome on Android does not use default MessageLoop. It has its own
  // Android specific MessageLoop.
  DCHECK(!main_message_loop_.get());

  // Create the MessageLoop if doesn't yet exist (and bind it to the native Java
  // loop). This is a critical point in the startup process.
  {
    TRACE_EVENT0("startup",
      "ChromeBrowserMainPartsAndroid::PreEarlyInitialization:CreateUiMsgLoop");
    if (!base::MessageLoopCurrent::IsSet())
      main_message_loop_ = std::make_unique<base::MessageLoopForUI>();
  }

  return ChromeBrowserMainParts::PreEarlyInitialization();
}

void ChromeBrowserMainPartsAndroid::PostBrowserStart() {
  ChromeBrowserMainParts::PostBrowserStart();

  base::PostDelayedTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&ReportSeccompSupport), base::TimeDelta::FromMinutes(1));

  RegisterChromeJavaMojoInterfaces();
}

void ChromeBrowserMainPartsAndroid::ShowMissingLocaleMessageBox() {
  NOTREACHED();
}

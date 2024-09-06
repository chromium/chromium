// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime_desktop.h"

#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/threading/hang_watcher.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_close_manager.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/metrics/shutdown_watcher_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder.h"
#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/profiles/profile_picker.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/browser_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_data_service.h"
#include "chrome/browser/sessions/session_data_service_factory.h"
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)

namespace chrome {

namespace {

base::RepeatingCallbackList<void(bool)>& GetClosingAllBrowsersCallbackList() {
  static base::NoDestructor<base::RepeatingCallbackList<void(bool)>>
      callback_list;
  return *callback_list;
}

using IgnoreUnloadHandlers =
    base::StrongAlias<class IgnoreUnloadHandlersTag, bool>;

void AttemptRestartInternal(IgnoreUnloadHandlers ignore_unload_handlers) {
  // TODO(beng): Can this use ProfileManager::GetLoadedProfiles instead?
  // TODO(crbug.com/40180622): Unset SaveSessionState if the restart fails.
  for (Browser* browser : *BrowserList::GetInstance()) {
    browser->profile()->SaveSessionState();
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
    auto* session_data_service =
        SessionDataServiceFactory::GetForProfile(browser->profile());
    if (session_data_service) {
      session_data_service->SetForceKeepSessionState();
    }
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)
  }

  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);
  KeepAliveRegistry::GetInstance()->SetRestarting();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(!chrome::IsSendingStopRequestToSessionManager());

  ash::BootTimesRecorder::Get()->set_restart_requested();
  chrome::SetSendStopRequestToSessionManager(false);

  // If an update is pending StopSession() will trigger a system reboot,
  // which in turn will send SIGTERM to Chrome, and that ends up processing
  // unload handlers.
  if (UpdatePending()) {
    browser_shutdown::NotifyAppTerminating();
    StopSession();
    return;
  }

  // Run exit process in clean stack.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ExitIgnoreUnloadHandlers));
#else  // !BUILDFLAG(IS_CHROMEOS_ASH).
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Request ash-chrome to relaunch Lacros on its process termination.
  // Do not set kRestartLastSessionOnShutdown for Lacros, because it tries to
  // respawn another Chrome process from the current Chrome process, which
  // does not work on Lacros.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::BrowserServiceHost>() &&
      lacros_service
              ->GetInterfaceVersion<crosapi::mojom::BrowserServiceHost>() >=
          static_cast<int>(
              crosapi::mojom::BrowserServiceHost::kRequestRelaunchMinVersion)) {
    lacros_service->GetRemote<crosapi::mojom::BrowserServiceHost>()
        ->RequestRelaunch();
  }
#else   // !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Set the flag to restore state after the restart.
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  if (ignore_unload_handlers) {
    ExitIgnoreUnloadHandlers();
  } else {
    AttemptExit();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShutdownIfNoBrowsers() {
  if (GetTotalBrowserCount() > 0) {
    return;
  }

  // Tell everyone that we are shutting down.
  browser_shutdown::SetTryingToQuit(true);

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  // If ShuttingDownWithoutClosingBrowsers() returns true, the session
  // services may not get a chance to shut down normally, so explicitly shut
  // them down here to ensure they have a chance to persist their data.
  ProfileManager::ShutdownSessionServices();
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)
  browser_shutdown::NotifyAppTerminating();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  StopSession();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  OnAppExiting();
}

}  // namespace

void CloseAllBrowsersAndQuit() {
  browser_shutdown::SetTryingToQuit(true);
  CloseAllBrowsers();
}

void CloseAllBrowsers() {
  // If there are no browsers and closing the last browser would quit the
  // application, send the APP_TERMINATING action here. Otherwise, it will be
  // sent by RemoveBrowser() when the last browser has closed.
  if (GetTotalBrowserCount() == 0 &&
      (browser_shutdown::IsTryingToQuit() ||
       !KeepAliveRegistry::GetInstance()->IsKeepingAlive())) {
    ShutdownIfNoBrowsers();
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::BootTimesRecorder::Get()->AddLogoutTimeMarker("StartedClosingWindows",
                                                     false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_refptr<BrowserCloseManager> browser_close_manager =
      new BrowserCloseManager;
  browser_close_manager->StartClosingBrowsers();
}

void AttemptRestart() {
  AttemptRestartInternal(IgnoreUnloadHandlers(false));
}
#if !BUILDFLAG(IS_CHROMEOS_ASH)
void RelaunchIgnoreUnloadHandlers() {
  AttemptRestartInternal(IgnoreUnloadHandlers(true));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void SessionEnding() {
  // This is a time-limited shutdown where we need to write as much to
  // disk as we can as soon as we can, and where we must kill the
  // process within a hang timeout to avoid user prompts.

  // EndSession is invoked once per frame. Only do something the first time.
  static bool already_ended = false;
  // We may get called in the middle of shutdown, e.g. https://crbug.com/70852
  // and https://crbug.com/1187418.  In this case, do nothing.
  if (already_ended || !g_browser_process) {
    return;
  }
  already_ended = true;

  // ~ShutdownWatcherHelper uses IO (it joins a thread). We'll only trigger that
  // if Terminate() fails, which leaves us in a weird state, or the OS is going
  // to kill us soon. Either way we don't care about that here.
  base::ScopedAllowBlocking allow_blocking;

  // Two different types of hang detection cannot attempt to upload crashes at
  // the same time or they would interfere with each other.
  std::optional<ShutdownWatcherHelper> shutdown_watcher;
  std::optional<base::WatchHangsInScope> watch_hangs_scope;
  if (base::HangWatcher::IsCrashReportingEnabled()) {
    // TODO(crbug.com/40840897): Migrate away from ShutdownWatcher and its old
    // timing.
    base::HangWatcher::SetShuttingDown();
    constexpr base::TimeDelta kShutdownHangDelay{base::Seconds(30)};
    watch_hangs_scope.emplace(kShutdownHangDelay);
  } else {
    // Start watching for hang during shutdown, and crash it if takes too long.
    // We disarm when |shutdown_watcher| object is destroyed, which is when we
    // exit this function.
    constexpr base::TimeDelta kShutdownHangDelay{base::Seconds(90)};
    shutdown_watcher.emplace();
    shutdown_watcher->Arm(kShutdownHangDelay);
  }

  browser_shutdown::OnShutdownStarting(
      browser_shutdown::ShutdownType::kEndSession);

  // In a clean shutdown, browser_shutdown::OnShutdownStarting sets
  // g_shutdown_type, and browser_shutdown::ShutdownPreThreadsStop calls
  // RecordShutdownInfoPrefs to update the pref with the value. However, here
  // the process is going to exit without calling ShutdownPreThreadsStop.
  // Instead, here we call RecordShutdownInfoPrefs to record the shutdown info.
  browser_shutdown::RecordShutdownInfoPrefs();

  OnClosingAllBrowsers(true);

  // Write important data first.
  g_browser_process->EndSession();

  // Emit the shutdown metric for the end-session case. The process will exit
  // after this point.
  browser_shutdown::RecordShutdownMetrics();

#if BUILDFLAG(IS_WIN)
  base::win::SetShouldCrashOnProcessDetach(false);
#endif  // BUILDFLAG(IS_WIN)

  // On Windows 7 and later, the system will consider the process ripe for
  // termination as soon as it hides or destroys its windows. Since any
  // execution past that point will be non-deterministically cut short, we
  // might as well put ourselves out of that misery deterministically.
  base::Process::TerminateCurrentProcessImmediately(0);
}

void ShutdownIfNeeded() {
  if (browser_shutdown::IsTryingToQuit()) {
    return;
  }

  ShutdownIfNoBrowsers();
}

void OnAppExiting() {
  static bool notified = false;
  if (notified) {
    return;
  }
  notified = true;
  HandleAppExitingForPlatform();
}

void OnClosingAllBrowsers(bool closing) {
  GetClosingAllBrowsersCallbackList().Notify(closing);
}

base::CallbackListSubscription AddClosingAllBrowsersCallback(
    base::RepeatingCallback<void(bool)> closing_all_browsers_callback) {
  return GetClosingAllBrowsersCallbackList().Add(
      std::move(closing_all_browsers_callback));
}

void MarkAsCleanShutdown() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  LogMarkAsCleanShutdown();
  // Tracks profiles that have pending write of the exit type.
  std::set<Profile*> pending_profiles;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (ExitTypeService* exit_type_service =
            ExitTypeService::GetInstanceForProfile(browser->profile())) {
      exit_type_service->SetCurrentSessionExitType(ExitType::kClean);

#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Explicitly schedule pending writes on ChromeOS so that even if the
      // UI thread is hosed (e.g. taking a long time to close all tabs because
      // of page faults/swap-in), the clean shutdown flag still gets a chance
      // to be persisted. See https://crbug.com/1294764
      Profile* profile = browser->profile();
      if (pending_profiles.insert(profile).second) {
        profile->GetPrefs()->CommitPendingWrite();
      }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    }
  }
}

bool AreAllBrowsersCloseable() {
  if (BrowserList::GetInstance()->empty()) {
    return true;
  }

  // If there are any downloads active, all browsers are not closeable.
  // However, this does not block for malicious downloads.
  if (DownloadCoreService::BlockingShutdownCountAllProfiles() > 0) {
    return false;
  }

  // Check TabsNeedBeforeUnloadFired().
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->TabsNeedBeforeUnloadFired()) {
      return false;
    }
  }
  return true;
}

}  // namespace chrome

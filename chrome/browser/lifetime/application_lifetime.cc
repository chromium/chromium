// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include <memory>
#include <string>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/bind.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/lifetime/browser_close_manager.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/user_manager.h"
#endif

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#endif

namespace chrome {
namespace {

#if !defined(OS_ANDROID)
// Returns true if all browsers can be closed without user interaction.
// This currently checks if there is pending download, or if it needs to
// handle unload handler.
bool AreAllBrowsersCloseable() {
  if (BrowserList::GetInstance()->empty())
    return true;

  // If there are any downloads active, all browsers are not closeable.
  // However, this does not block for malicious downloads.
  if (DownloadCoreService::NonMaliciousDownloadCountAllProfiles() > 0)
    return false;

  // Check TabsNeedBeforeUnloadFired().
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->TabsNeedBeforeUnloadFired())
      return false;
  }
  return true;
}
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
// Sets kApplicationLocale in |local_state| for the login screen on the next
// application start, if it is forced to a specific value due to enterprise
// policy or the owner's locale.  Returns true if any pref has been modified.
bool SetLocaleForNextStart(PrefService* local_state) {
  // If a policy mandates the login screen locale, use it.
  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
  const base::ListValue* login_screen_locales = nullptr;
  std::string login_screen_locale;
  if (cros_settings->GetList(chromeos::kDeviceLoginScreenLocales,
                             &login_screen_locales) &&
      !login_screen_locales->empty() &&
      login_screen_locales->GetString(0, &login_screen_locale)) {
    local_state->SetString(language::prefs::kApplicationLocale,
                           login_screen_locale);
    return true;
  }

  // Login screen should show up in owner's locale.
  std::string owner_locale = local_state->GetString(prefs::kOwnerLocale);
  std::string pref_locale =
      local_state->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&pref_locale);
  if (!owner_locale.empty() && pref_locale != owner_locale &&
      !local_state->IsManagedPreference(language::prefs::kApplicationLocale)) {
    local_state->SetString(language::prefs::kApplicationLocale, owner_locale);
    return true;
  }

  return false;
}

// Whether chrome should send stop request to a session manager.
bool g_send_stop_request_to_session_manager = false;
#endif

}  // namespace

#if !defined(OS_ANDROID)
void MarkAsCleanShutdown() {
  // TODO(beng): Can this use ProfileManager::GetLoadedProfiles() instead?
  for (auto* browser : *BrowserList::GetInstance())
    browser->profile()->SetExitType(Profile::EXIT_NORMAL);
}
#endif

void AttemptExitInternal(bool try_to_quit_application) {
  // On Mac, the platform-specific part handles setting this.
#if !defined(OS_MACOSX)
  if (try_to_quit_application)
    browser_shutdown::SetTryingToQuit(true);
#endif

  content::NotificationService::current()->Notify(
      NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  g_browser_process->platform_part()->AttemptExit();
}

#if !defined(OS_ANDROID)
void CloseAllBrowsersAndQuit() {
  browser_shutdown::SetTryingToQuit(true);
  CloseAllBrowsers();
}

void ShutdownIfNoBrowsers() {
  if (GetTotalBrowserCount() > 0)
    return;

  // Tell everyone that we are shutting down.
  browser_shutdown::SetTryingToQuit(true);

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  // If ShuttingDownWithoutClosingBrowsers() returns true, the session
  // services may not get a chance to shut down normally, so explicitly shut
  // them down here to ensure they have a chance to persist their data.
  ProfileManager::ShutdownSessionServices();
#endif

  browser_shutdown::NotifyAndTerminate(true /* fast_path */);
  OnAppExiting();
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

#if defined(OS_CHROMEOS)
  chromeos::BootTimesRecorder::Get()->AddLogoutTimeMarker(
      "StartedClosingWindows", false);
#endif
  scoped_refptr<BrowserCloseManager> browser_close_manager =
      new BrowserCloseManager;
  browser_close_manager->StartClosingBrowsers();
}
#endif  // !defined(OS_ANDROID)

void AttemptUserExit() {
#if defined(OS_CHROMEOS)
  VLOG(1) << "AttemptUserExit";
  browser_shutdown::StartShutdownTracing();
  chromeos::BootTimesRecorder::Get()->AddLogoutTimeMarker("LogoutStarted",
                                                          false);

  PrefService* state = g_browser_process->local_state();
  if (state) {
    chromeos::BootTimesRecorder::Get()->OnLogoutStarted(state);

    if (SetLocaleForNextStart(state)) {
      TRACE_EVENT0("shutdown", "CommitPendingWrite");
      state->CommitPendingWrite();
    }
  }
  g_send_stop_request_to_session_manager = true;
  // On ChromeOS, always terminate the browser, regardless of the result of
  // AreAllBrowsersCloseable(). See crbug.com/123107.
  browser_shutdown::NotifyAndTerminate(true /* fast_path */);
#else
  // Reset the restart bit that might have been set in cancelled restart
  // request.
#if !defined(OS_ANDROID)
  UserManager::Hide();
#endif
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, false);
  AttemptExitInternal(false);
#endif  // defined(OS_CHROMEOS)
}

// The Android implementation is in application_lifetime_android.cc
#if !defined(OS_ANDROID)
void AttemptRestart() {
#if defined(OS_WIN)
  // On Windows, Breakpad will upload crash reports if the breakpad pipe name
  // environment variable is defined. So we undefine this environment variable
  // before restarting, as the restarted processes will inherit their
  // environment variables from ours, thus suppressing crash uploads.
  if (!ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled()) {
    HMODULE exe_module = GetModuleHandle(kBrowserProcessExecutableName);
    if (exe_module) {
      typedef void (__cdecl *ClearBreakpadPipeEnvVar)();
      ClearBreakpadPipeEnvVar clear = reinterpret_cast<ClearBreakpadPipeEnvVar>(
          GetProcAddress(exe_module, "ClearBreakpadPipeEnvironmentVariable"));
      if (clear)
        clear();
    }
  }
#endif  // defined(OS_WIN)

  // TODO(beng): Can this use ProfileManager::GetLoadedProfiles instead?
  for (auto* browser : *BrowserList::GetInstance())
    content::BrowserContext::SaveSessionState(browser->profile());

  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

#if defined(OS_CHROMEOS)
  chromeos::BootTimesRecorder::Get()->set_restart_requested();

  DCHECK(!g_send_stop_request_to_session_manager);
  // Make sure we don't send stop request to the session manager.
  g_send_stop_request_to_session_manager = false;
  // Run exit process in clean stack.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::Bind(&ExitCleanly));
#else
  // Set the flag to restore state after the restart.
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
  AttemptExit();
#endif
}
#endif  // !defined(OS_ANDROID)

void AttemptRelaunch() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER, "Chrome relaunch");
  // If running the Chrome OS build, but we're not on the device, fall through.
#endif
  AttemptRestart();
}

void AttemptExit() {
#if defined(OS_CHROMEOS)
  // On ChromeOS, user exit and system exits are the same.
  AttemptUserExit();
#else
  // If we know that all browsers can be closed without blocking,
  // don't notify users of crashes beyond this point.
  // Note that MarkAsCleanShutdown() does not set UMA's exit cleanly bit
  // so crashes during shutdown are still reported in UMA.
#if !defined(OS_ANDROID)
  // Android doesn't use Browser.
  if (AreAllBrowsersCloseable())
    MarkAsCleanShutdown();
#endif
  AttemptExitInternal(true);
#endif
}

#if defined(OS_CHROMEOS)
// A function called when SIGTERM is received.
void ExitCleanly() {
  VLOG(1) << "ExitCleanly";
  // We always mark exit cleanly.
  MarkAsCleanShutdown();

  // Don't block when SIGTERM is received. AreaAllBrowsersCloseable()
  // can be false in following cases. a) power-off b) signout from
  // screen locker.
  if (!AreAllBrowsersCloseable())
    browser_shutdown::OnShutdownStarting(browser_shutdown::END_SESSION);
  else
    browser_shutdown::OnShutdownStarting(browser_shutdown::BROWSER_EXIT);
  AttemptExitInternal(true);
}

bool IsAttemptingShutdown() {
  return g_send_stop_request_to_session_manager;
}
#endif

#if !defined(OS_ANDROID)
void SessionEnding() {
  // This is a time-limited shutdown where we need to write as much to
  // disk as we can as soon as we can, and where we must kill the
  // process within a hang timeout to avoid user prompts.

  // EndSession is invoked once per frame. Only do something the first time.
  static bool already_ended = false;
  // We may get called in the middle of shutdown, e.g. http://crbug.com/70852
  // In this case, do nothing.
  if (already_ended || !content::NotificationService::current())
    return;
  already_ended = true;

  // ~ShutdownWatcherHelper uses IO (it joins a thread). We'll only trigger that
  // if Terminate() fails, which leaves us in a weird state, or the OS is going
  // to kill us soon. Either way we don't care about that here.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // Start watching for hang during shutdown, and crash it if takes too long.
  // We disarm when |shutdown_watcher| object is destroyed, which is when we
  // exit this function.
  ShutdownWatcherHelper shutdown_watcher;
  shutdown_watcher.Arm(base::TimeDelta::FromSeconds(90));
  metrics::MetricsService::SetExecutionPhase(
      metrics::ExecutionPhase::SHUTDOWN_TIMEBOMB_ARM,
      g_browser_process->local_state());

  browser_shutdown::OnShutdownStarting(browser_shutdown::END_SESSION);

  // In a clean shutdown, browser_shutdown::OnShutdownStarting sets
  // g_shutdown_type, and browser_shutdown::ShutdownPreThreadsStop calls
  // RecordShutdownInfoPrefs to update the pref with the value. However, here
  // the process is going to exit without calling ShutdownPreThreadsStop.
  // Instead, here we call RecordShutdownInfoPrefs to record the shutdown info.
  browser_shutdown::RecordShutdownInfoPrefs();

  content::NotificationService::current()->Notify(
      NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  // Write important data first.
  g_browser_process->EndSession();

#if defined(OS_WIN)
  base::win::SetShouldCrashOnProcessDetach(false);
#endif
  // On Windows 7 and later, the system will consider the process ripe for
  // termination as soon as it hides or destroys its windows. Since any
  // execution past that point will be non-deterministically cut short, we
  // might as well put ourselves out of that misery deterministically.
  base::Process::TerminateCurrentProcessImmediately(0);
}

void ShutdownIfNeeded() {
  if (browser_shutdown::IsTryingToQuit())
    return;

  ShutdownIfNoBrowsers();
}

void OnAppExiting() {
  static bool notified = false;
  if (notified)
    return;
  notified = true;
  HandleAppExitingForPlatform();
}
#endif  // !defined(OS_ANDROID)

}  // namespace chrome

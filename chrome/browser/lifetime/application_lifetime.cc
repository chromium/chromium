// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/lifetime/browser_close_manager.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/aura/env.h"
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/profile_picker.h"
#endif

#if defined(OS_WIN)
#include "base/win/win_util.h"
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

base::RepeatingCallbackList<void(bool)>& GetClosingAllBrowsersCallbackList() {
  static base::NoDestructor<base::RepeatingCallbackList<void(bool)>>
      callback_list;
  return *callback_list;
}
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Sets kApplicationLocale in |local_state| for the login screen on the next
// application start, if it is forced to a specific value due to enterprise
// policy or the owner's locale.  Returns true if any pref has been modified.
bool SetLocaleForNextStart(PrefService* local_state) {
  // If a policy mandates the login screen locale, use it.
  ash::CrosSettings* cros_settings = ash::CrosSettings::Get();
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

#if !defined(OS_ANDROID)
using IgnoreUnloadHandlers =
    base::StrongAlias<class IgnoreUnloadHandlersTag, bool>;

void AttemptRestartInternal(IgnoreUnloadHandlers ignore_unload_handlers) {
  // TODO(beng): Can this use ProfileManager::GetLoadedProfiles instead?
  for (auto* browser : *BrowserList::GetInstance())
    content::BrowserContext::SaveSessionState(browser->profile());

  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::BootTimesRecorder::Get()->set_restart_requested();

  DCHECK(!g_send_stop_request_to_session_manager);
  // Make sure we don't send stop request to the session manager.
  g_send_stop_request_to_session_manager = false;
  // Run exit process in clean stack.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ExitIgnoreUnloadHandlers));
#else
  // Set the flag to restore state after the restart.
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
  if (ignore_unload_handlers)
    ExitIgnoreUnloadHandlers();
  else
    AttemptExit();
#endif
}
#endif  // !defined(OS_ANDROID)

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
#if !defined(OS_MAC)
  if (try_to_quit_application)
    browser_shutdown::SetTryingToQuit(true);
#endif

#if !defined(OS_ANDROID)
  OnClosingAllBrowsers(true);
#endif

  g_browser_process->platform_part()->AttemptExit(try_to_quit_application);
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::BootTimesRecorder::Get()->AddLogoutTimeMarker(
      "StartedClosingWindows", false);
#endif
  scoped_refptr<BrowserCloseManager> browser_close_manager =
      new BrowserCloseManager;
  browser_close_manager->StartClosingBrowsers();
}
#endif  // !defined(OS_ANDROID)

void AttemptUserExit() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  VLOG(1) << "AttemptUserExit";
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
  ProfilePicker::Hide();
#endif
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, false);
  AttemptExitInternal(false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// The Android implementation is in application_lifetime_android.cc
#if !defined(OS_ANDROID)
void AttemptRestart() {
  AttemptRestartInternal(IgnoreUnloadHandlers(false));
}
#endif  // !defined(OS_ANDROID)

void AttemptRelaunch() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER, "Chrome relaunch");
  // If running the Chrome OS build, but we're not on the device, fall through.
#endif
  AttemptRestart();
}

#if !defined(OS_ANDROID)
void RelaunchIgnoreUnloadHandlers() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER, "Chrome relaunch");
  // If running the Chrome OS build, but we're not on the device, fall through.
#endif
  AttemptRestartInternal(IgnoreUnloadHandlers(true));
}
#endif

void AttemptExit() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
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

void ExitIgnoreUnloadHandlers() {
  VLOG(1) << "ExitIgnoreUnloadHandlers";
#if !defined(OS_ANDROID)
  // We always mark exit cleanly.
  MarkAsCleanShutdown();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Disable window occlusion tracking on exit before closing all browser
  // windows to make shutdown faster. Note that the occlusion tracking is
  // paused indefinitely. It is okay do so on Chrome OS because there is
  // no way to abort shutdown and go back to user sessions at this point.
  DCHECK(aura::Env::HasInstance());
  aura::Env::GetInstance()->PauseWindowOcclusionTracking();

  // On ChromeOS ExitIgnoreUnloadHandlers() is used to handle SIGTERM.
  // In this case, AreAllBrowsersCloseable()
  // can be false in following cases. a) power-off b) signout from
  // screen locker.
  browser_shutdown::OnShutdownStarting(
      AreAllBrowsersCloseable() ? browser_shutdown::ShutdownType::kBrowserExit
                                : browser_shutdown::ShutdownType::kEndSession);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  // For desktop browsers, always perform a silent exit.
  browser_shutdown::OnShutdownStarting(
      browser_shutdown::ShutdownType::kSilentExit);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // !defined(OS_ANDROID)
  AttemptExitInternal(true);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  // We may get called in the middle of shutdown, e.g. https://crbug.com/70852
  // and https://crbug.com/1187418.  In this case, do nothing.
  if (already_ended || !content::NotificationService::current() ||
      !g_browser_process) {
    return;
  }
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

void OnClosingAllBrowsers(bool closing) {
  GetClosingAllBrowsersCallbackList().Notify(closing);
}

base::CallbackListSubscription AddClosingAllBrowsersCallback(
    base::RepeatingCallback<void(bool)> closing_all_browsers_callback) {
  return GetClosingAllBrowsersCallbackList().Add(
      std::move(closing_all_browsers_callback));
}
#endif  // !defined(OS_ANDROID)

}  // namespace chrome

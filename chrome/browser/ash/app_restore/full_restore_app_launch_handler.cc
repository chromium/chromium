// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"

#include <set>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/app_restore/arc_app_queue_restore_handler.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service_log.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/common/chrome_switches.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/app_restore/full_restore_save_handler.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash::full_restore {

namespace {

bool g_launch_browser_for_testing = false;

constexpr char kRestoredAppLaunchHistogramPrefix[] = "Apps.RestoredAppLaunch";
constexpr char kRestoreBrowserResultHistogramPrefix[] =
    "Apps.RestoreBrowserResult";
constexpr char kSessionRestoreExitResultPrefix[] =
    "Apps.SessionRestoreExitResult";
constexpr char kSessionRestoreWindowCountPrefix[] =
    "Apps.SessionRestoreWindowCount";
constexpr char kFullRestoreTabCountPrefix[] = "Apps.FullRestoreTabCount";

}  // namespace

FullRestoreAppLaunchHandler::FullRestoreAppLaunchHandler(
    Profile* profile,
    bool should_init_service)
    : AppLaunchHandler(profile), should_init_service_(should_init_service) {
  // FullRestoreReadHandler reads the full restore data from the full restore
  // data file on a background task runner.
  ::full_restore::FullRestoreReadHandler::GetInstance()->ReadFromFile(
      profile->GetPath(),
      base::BindOnce(&FullRestoreAppLaunchHandler::OnGetRestoreData,
                     weak_ptr_factory_.GetWeakPtr()));
}

FullRestoreAppLaunchHandler::~FullRestoreAppLaunchHandler() = default;

// TODO: b/325616600 - Move early returns for floating workspace service checks
// logic out.
void FullRestoreAppLaunchHandler::LaunchBrowserWhenReady(
    bool first_run_full_restore) {
  if (floating_workspace_util::ShouldHandleRestartRestore()) {
    return;
  }

  if (g_launch_browser_for_testing ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceLaunchBrowser)) {
    ForceLaunchBrowserForTesting();
    return;
  }

  if (first_run_full_restore) {
    // Observe AppRegistryCache to get the notification when the app is ready.
    if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
            profile())) {
      auto* cache = &apps::AppServiceProxyFactory::GetForProfile(profile())
                         ->AppRegistryCache();
      ObserveCache(cache);

      for (const auto app_type : cache->InitializedAppTypes()) {
        OnAppTypeInitialized(app_type);
      }
    }

    LaunchBrowserForFirstRunFullRestore();
    return;
  }

  // If the restore data has been loaded, and the user has chosen to restore,
  // launch the browser.
  if (CanLaunchBrowser()) {
    LaunchBrowser();

    // OS Setting should be launched after browser to have OS setting window in
    // front.
    UserSessionManager::GetInstance()->PerformPostBrowserLaunchOOBEActions(
        profile());
    return;
  }

  UserSessionManager::GetInstance()->PerformPostBrowserLaunchOOBEActions(
      profile());

  // If the restore data hasn't been loaded, or the user hasn't chosen to
  // restore, set `should_launch_browser_` as true, and wait the restore data
  // loaded, and the user selection, then we can launch the browser.
  should_launch_browser_ = true;
}

void FullRestoreAppLaunchHandler::SetShouldRestore() {
  should_restore_ = true;
  MaybePostRestore();
}

bool FullRestoreAppLaunchHandler::IsRestoreDataLoaded() {
  return restore_data() != nullptr;
}

void FullRestoreAppLaunchHandler::OnAppUpdate(const apps::AppUpdate& update) {
  // If the restore flag `should_restore_` is true, launch the app for
  // restoration.
  if (should_restore_)
    AppLaunchHandler::OnAppUpdate(update);
}

void FullRestoreAppLaunchHandler::OnAppTypeInitialized(apps::AppType app_type) {
  if (app_type == apps::AppType::kChromeApp) {
    are_chrome_apps_initialized_ = true;
    return;
  }

  if (app_type != apps::AppType::kWeb)
    return;

  are_web_apps_initialized_ = true;

  // `are_web_apps_initialized_` is checked in MaybeStartSaveTimer to decide
  // whether start the save timer. So if web apps are ready, call
  // MaybeStartSaveTimer to start the save timer if possbile.
  MaybeStartSaveTimer();

  if (first_run_full_restore_) {
    LaunchBrowserForFirstRunFullRestore();
    return;
  }

  if (should_launch_browser_ && CanLaunchBrowser()) {
    LaunchBrowser();
    should_launch_browser_ = false;
  }
}

void FullRestoreAppLaunchHandler::OnGotSession(Profile* session_profile,
                                               bool for_app,
                                               int window_count) {
  if (session_profile != profile())
    return;

  if (for_app)
    browser_app_window_count_ = window_count;
  else
    browser_window_count_ = window_count;
}

void FullRestoreAppLaunchHandler::OnMojoDisconnected() {
  observation_.Reset();
}

void FullRestoreAppLaunchHandler::OnStateChanged() {
  if (crosapi::BrowserManager::Get()->IsRunning()) {
    observation_.Reset();
    if (!floating_workspace_util::ShouldHandleRestartRestore()) {
      VLOG(1) << "Full restore opens Lacros";
      crosapi::BrowserManager::Get()->OpenForFullRestore(
          /*skip_crash_restore=*/IsLastSessionExitTypeCrashed());
    }
  }
}

void FullRestoreAppLaunchHandler::ForceLaunchBrowserForTesting() {
  ::full_restore::AddChromeBrowserLaunchInfoForTesting(profile()->GetPath());
  UserSessionManager::GetInstance()->LaunchBrowser(profile());
  UserSessionManager::GetInstance()->PerformPostBrowserLaunchOOBEActions(
      profile());
}

void FullRestoreAppLaunchHandler::OnExtensionLaunching(
    const std::string& app_id) {
  ::full_restore::FullRestoreReadHandler::GetInstance()
      ->SetNextRestoreWindowIdForChromeApp(profile()->GetPath(), app_id);
}

base::WeakPtr<AppLaunchHandler>
FullRestoreAppLaunchHandler::GetWeakPtrAppLaunchHandler() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FullRestoreAppLaunchHandler::OnGetRestoreData(
    std::unique_ptr<::app_restore::RestoreData> restore_data) {
  set_restore_data(std::move(restore_data));
  LogRestoreData();

  // FullRestoreAppLaunchHandler could be created multiple times in browser
  // tests, and used by the desk template. Only when it is created by
  // FullRestoreService, we need to init FullRestoreService.
  bool is_full_restore_shown = false;
  if (should_init_service_) {
    FullRestoreServiceFactory::GetForProfile(profile())->Init(
        is_full_restore_shown);
  }

  policy::RebootNotificationsScheduler* reboot_notifications_scheduler =
      policy::RebootNotificationsScheduler::Get();
  if (reboot_notifications_scheduler) {
    reboot_notifications_scheduler->MaybeShowPostRebootNotification(
        !is_full_restore_shown);
  }
}

void FullRestoreAppLaunchHandler::MaybePostRestore() {
  MaybeStartSaveTimer();

  // If the restore flag `should_restore_` is not true, or reading the restore
  // data hasn't finished, don't restore.
  if (!should_restore_ || !restore_data())
    return;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FullRestoreAppLaunchHandler::MaybeRestore,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FullRestoreAppLaunchHandler::MaybeRestore() {
  if (floating_workspace_util::ShouldHandleRestartRestore()) {
    return;
  }
  ::full_restore::FullRestoreReadHandler::GetInstance()->SetStartTimeForProfile(
      profile()->GetPath());
  ::full_restore::FullRestoreReadHandler::GetInstance()->SetCheckRestoreData(
      profile()->GetPath());

  if (should_launch_browser_ && CanLaunchBrowser()) {
    LaunchBrowser();
    should_launch_browser_ = false;
  }

  VLOG(1) << "Restore apps in " << profile()->GetPath();
  if (auto* arc_task_handler =
          app_restore::AppRestoreArcTaskHandlerFactory::GetForProfile(
              profile())) {
    arc_task_handler->GetFullRestoreArcAppQueueRestoreHandler()->RestoreArcApps(
        this);
  }

  MaybeRestoreLacros();

  LaunchApps();

  MaybeStartSaveTimer();
}

bool FullRestoreAppLaunchHandler::CanLaunchBrowser() {
  return should_restore_ && restore_data() &&
         (!restore_data()->HasAppTypeBrowser() || are_web_apps_initialized_);
}

void FullRestoreAppLaunchHandler::LaunchBrowser() {
  // If the browser is not launched before reboot, don't launch browser during
  // the startup phase.
  const auto& launch_list = restore_data()->app_id_to_launch_list();
  if (launch_list.find(app_constants::kChromeAppId) == launch_list.end())
    return;

  SessionRestore::AddObserver(this);

  VLOG(1) << "Restore browser for " << profile()->GetPath();
  RecordRestoredAppLaunch(apps::AppTypeName::kChromeBrowser);

  restore_data()->RemoveApp(app_constants::kChromeAppId);

  if (IsLastSessionExitTypeCrashed()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kHideCrashRestoreBubble);
  }

  MaybeStartSaveTimer();

  if (!::full_restore::HasBrowser(profile()->GetPath())) {
    // If there is no normal browsers before reboot, call session restore to
    // restore app type browsers only.
    SessionRestore::RestoreSession(profile(), nullptr,
                                   SessionRestore::RESTORE_APPS, StartupTabs());
    SessionRestore::RemoveObserver(this);
    return;
  }

  // Modify the command line to restore browser sessions.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kRestoreLastSession);

  UserSessionManager::GetInstance()->LaunchBrowser(profile());
  RecordLaunchBrowserResult();
  SessionRestore::RemoveObserver(this);
}

void FullRestoreAppLaunchHandler::LaunchBrowserForFirstRunFullRestore() {
  first_run_full_restore_ = true;

  // Wait for the web apps initialized. Because the app type in AppRegistryCache
  // is checked when save the browser window. If the app doesn't exist in
  // AppRegistryCache, the web app window can't be saved in the full restore
  // file, which could affect the restoration next time after reboot.
  if (!are_web_apps_initialized_)
    return;

  first_run_full_restore_ = false;

  UserSessionManager::GetInstance()->LaunchBrowser(profile());

  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  SessionStartupPref session_startup_pref =
      SessionStartupPref::GetStartupPref(prefs);

  // If the system is upgrading from a crash, the app type browser window can be
  // restored, so we don't need to call session restore to restore app type
  // browsers. If the session restore setting is not restore, we don't need to
  // restore app type browser neither.
  if (ExitTypeService::GetLastSessionExitType(profile()) !=
          ExitType::kCrashed &&
      !::full_restore::HasAppTypeBrowser(profile()->GetPath()) &&
      session_startup_pref.ShouldRestoreLastSession()) {
    StartupTabs startup_tabs;
    if (session_startup_pref.type == SessionStartupPref::LAST_AND_URLS)
      startup_tabs = session_startup_pref.ToStartupTabs();
    // Restore the app type browsers only when the web apps are ready.
    SessionRestore::RestoreSession(profile(), nullptr,
                                   SessionRestore::RESTORE_APPS, startup_tabs);
  }

  UserSessionManager::GetInstance()->PerformPostBrowserLaunchOOBEActions(
      profile());
}

void FullRestoreAppLaunchHandler::MaybeRestoreLacros() {
  if (!crosapi::browser_util::IsLacrosEnabled() ||
      !::full_restore::features::IsFullRestoreForLacrosEnabled()) {
    return;
  }

  // TODO(crbug.com/40194081):
  // 1. Modify the restore conditions, e.g. check web apps ready, etc.
  // 2. Handle the migration scenario, e.g. from flag disable to enable.
  // 3. Add metrics to check whether the Lacros is restored successfully.
  if (!base::Contains(restore_data()->app_id_to_launch_list(),
                      app_constants::kLacrosAppId)) {
    return;
  }

  restore_data()->RemoveApp(app_constants::kLacrosAppId);

  if (crosapi::BrowserManager::Get()->IsRunning()) {
    VLOG(1) << "Full restore opens Lacros";
    crosapi::BrowserManager::Get()->OpenForFullRestore(
        /*skip_crash_restore=*/IsLastSessionExitTypeCrashed());
    return;
  }

  if (!crosapi::BrowserManager::Get()->IsTerminated())
    observation_.Observe(crosapi::BrowserManager::Get());
}

void FullRestoreAppLaunchHandler::RecordRestoredAppLaunch(
    apps::AppTypeName app_type_name) {
  base::UmaHistogramEnumeration(kRestoredAppLaunchHistogramPrefix,
                                app_type_name);
}

void FullRestoreAppLaunchHandler::RecordLaunchBrowserResult() {
  RestoreTabResult result = RestoreTabResult::kNoTabs;

  int window_count = 0;
  int tab_count = 0;
  std::list<SessionServiceEvent> events = GetSessionServiceEvents(profile());
  if (!events.empty()) {
    auto it = events.back();
    if (it.type == SessionServiceEventLogType::kRestore) {
      window_count = it.data.restore.window_count;
      tab_count = it.data.exit.tab_count;
      if (tab_count > 0)
        result = RestoreTabResult::kHasTabs;
    } else {
      result = RestoreTabResult::kError;
      window_count = -1;
      tab_count = -1;
    }
  }

  VLOG(1) << "Browser is restored (windows=" << window_count
          << " tabs=" << tab_count << ").";
  base::UmaHistogramEnumeration(kRestoreBrowserResultHistogramPrefix, result);
  base::UmaHistogramCounts100(kFullRestoreTabCountPrefix, tab_count);

  if (result != RestoreTabResult::kNoTabs)
    return;

  SessionRestoreExitResult session_restore_exit =
      SessionRestoreExitResult::kNoExit;
  for (auto iter = events.rbegin(); iter != events.rend(); ++iter) {
    if (iter->type != SessionServiceEventLogType::kStart)
      continue;

    ++iter;
    if (iter != events.rend() &&
        iter->type == SessionServiceEventLogType::kExit) {
      bool is_first_session_service = iter->data.exit.is_first_session_service;
      bool did_schedule_command = iter->data.exit.did_schedule_command;
      if (is_first_session_service) {
        session_restore_exit =
            did_schedule_command
                ? SessionRestoreExitResult::kIsFirstServiceDidSchedule
                : SessionRestoreExitResult::kIsFirstServiceNoSchedule;
      } else {
        session_restore_exit =
            did_schedule_command
                ? SessionRestoreExitResult::kNotFirstServiceDidSchedule
                : SessionRestoreExitResult::kNotFirstServiceNoSchedule;
      }
    }
    break;
  }

  base::UmaHistogramEnumeration(kSessionRestoreExitResultPrefix,
                                session_restore_exit);

  SessionRestoreWindowCount restored_window_count;
  if (browser_app_window_count_ != 0) {
    restored_window_count =
        browser_window_count_ == 0
            ? SessionRestoreWindowCount::kHasAppWindowNoNormalWindow
            : SessionRestoreWindowCount::kHasAppWindowHasNormalWindow;
  } else {
    restored_window_count =
        browser_window_count_ == 0
            ? SessionRestoreWindowCount::kNoWindow
            : SessionRestoreWindowCount::kNoAppWindowHasNormalWindow;
  }
  base::UmaHistogramEnumeration(kSessionRestoreWindowCountPrefix,
                                restored_window_count);
}

void FullRestoreAppLaunchHandler::LogRestoreData() {
  if (!restore_data() || restore_data()->app_id_to_launch_list().empty()) {
    VLOG(1) << "There is no restore data from " << profile()->GetPath();
    return;
  }

  int arc_app_count = 0;
  int other_app_count = 0;
  for (const auto& it : restore_data()->app_id_to_launch_list()) {
    if (it.first == app_constants::kChromeAppId || it.second.empty())
      continue;

    if (it.second.begin()->second->event_flag.has_value()) {
      ++arc_app_count;
      continue;
    }

    ++other_app_count;
  }

  VLOG(1) << "There is restore data: Browser("
          << (::full_restore::HasAppTypeBrowser(profile()->GetPath())
                  ? " has app type browser "
                  : " no app type browser")
          << ","
          << (::full_restore::HasBrowser(profile()->GetPath())
                  ? " has normal browser "
                  : " no normal ")
          << ") ARC(" << arc_app_count << ") other apps(" << other_app_count
          << ") in " << profile()->GetPath();
}

void FullRestoreAppLaunchHandler::MaybeStartSaveTimer() {
  if (!should_restore_) {
    // FullRestoreService is responsible to handle all non restore processes.
    return;
  }

  if (!restore_data() || restore_data()->app_id_to_launch_list().empty()) {
    // If there is no restore data, start the timer.
    ::full_restore::FullRestoreSaveHandler::GetInstance()->AllowSave();
    return;
  }

  if (base::Contains(restore_data()->app_id_to_launch_list(),
                     app_constants::kChromeAppId)) {
    // If the browser hasn't been restored yet, Wait for the browser
    // restoration. LaunchBrowser will call this function again to start the
    // save timer after restore the browser sessions.
    return;
  }

  // If both web apps and chrome apps has finished the initialization, start the
  // timer.
  if (are_chrome_apps_initialized_ && are_web_apps_initialized_)
    ::full_restore::FullRestoreSaveHandler::GetInstance()->AllowSave();
}

bool FullRestoreAppLaunchHandler::IsLastSessionExitTypeCrashed() {
  return ExitTypeService::GetLastSessionExitType(profile()) ==
         ExitType::kCrashed;
}

ScopedLaunchBrowserForTesting::ScopedLaunchBrowserForTesting() {
  g_launch_browser_for_testing = true;
}

ScopedLaunchBrowserForTesting::~ScopedLaunchBrowserForTesting() {
  g_launch_browser_for_testing = false;
}

}  // namespace ash::full_restore

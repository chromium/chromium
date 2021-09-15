// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/full_restore/full_restore_app_launch_handler.h"

#include <set>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/full_restore/arc_app_launch_handler.h"
#include "chrome/browser/ash/full_restore/full_restore_arc_task_handler.h"
#include "chrome/browser/ash/full_restore/full_restore_service.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service_log.h"
#include "chrome/common/chrome_switches.h"
#include "components/full_restore/full_restore_read_handler.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "extensions/common/constants.h"

namespace ash {
namespace full_restore {

namespace {

bool g_launch_browser_for_testing = false;

constexpr char kRestoredAppLaunchHistogramPrefix[] = "Apps.RestoredAppLaunch";
constexpr char kRestoreBrowserResultHistogramPrefix[] =
    "Apps.RestoreBrowserResult";

}  // namespace

FullRestoreAppLaunchHandler::FullRestoreAppLaunchHandler(
    Profile* profile,
    bool should_init_service)
    : AppLaunchHandler(profile), should_init_service_(should_init_service) {
  // FullRestoreReadHandler reads the full restore data from the full restore
  // data file on a background task runner.
  ::full_restore::FullRestoreReadHandler::GetInstance()->ReadFromFile(
      profile_->GetPath(),
      base::BindOnce(&FullRestoreAppLaunchHandler::OnGetRestoreData,
                     weak_ptr_factory_.GetWeakPtr()));
}

FullRestoreAppLaunchHandler::~FullRestoreAppLaunchHandler() = default;

void FullRestoreAppLaunchHandler::LaunchBrowserWhenReady(
    bool first_run_full_restore) {
  if (g_launch_browser_for_testing ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceLaunchBrowser)) {
    ForceLaunchBrowserForTesting();
    return;
  }

  if (first_run_full_restore) {
    // Observe AppRegistryCache to get the notification when the app is ready.
    if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
            profile_)) {
      auto* cache = &apps::AppServiceProxyFactory::GetForProfile(profile_)
                         ->AppRegistryCache();
      Observe(cache);

      for (const auto app_type : cache->GetInitializedAppTypes()) {
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
    UserSessionManager::GetInstance()->MaybeLaunchSettings(profile_);
    return;
  }

  UserSessionManager::GetInstance()->MaybeLaunchSettings(profile_);

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
  return restore_data_ != nullptr;
}

void FullRestoreAppLaunchHandler::OnAppUpdate(const apps::AppUpdate& update) {
  // If the restore flag `should_restore_` is true, launch the app for
  // restoration.
  if (should_restore_)
    AppLaunchHandler::OnAppUpdate(update);
}

void FullRestoreAppLaunchHandler::OnAppTypeInitialized(
    apps::mojom::AppType app_type) {
  if (app_type == apps::mojom::AppType::kExtension) {
    are_chrome_apps_initialized_ = true;
    return;
  }

  if (app_type != apps::mojom::AppType::kWeb)
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

void FullRestoreAppLaunchHandler::ForceLaunchBrowserForTesting() {
  ::full_restore::AddChromeBrowserLaunchInfoForTesting(profile_->GetPath());
  UserSessionManager::GetInstance()->LaunchBrowser(profile_);
  UserSessionManager::GetInstance()->MaybeLaunchSettings(profile_);
}

void FullRestoreAppLaunchHandler::OnExtensionLaunching(
    const std::string& app_id) {
  ::full_restore::FullRestoreReadHandler::GetInstance()
      ->SetNextRestoreWindowIdForChromeApp(profile_->GetPath(), app_id);
}

base::WeakPtr<AppLaunchHandler>
FullRestoreAppLaunchHandler::GetWeakPtrAppLaunchHandler() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FullRestoreAppLaunchHandler::OnGetRestoreData(
    std::unique_ptr<::full_restore::RestoreData> restore_data) {
  restore_data_ = std::move(restore_data);
  LogRestoreData();

  // FullRestoreAppLaunchHandler could be created multiple times in browser
  // tests, and used by the desk template. Only when it is created by
  // FullRestoreService, we need to init FullRestoreService.
  if (should_init_service_)
    FullRestoreService::GetForProfile(profile_)->Init();
}

void FullRestoreAppLaunchHandler::MaybePostRestore() {
  MaybeStartSaveTimer();

  // If the restore flag `should_restore_` is not true, or reading the restore
  // data hasn't finished, don't restore.
  if (!should_restore_ || !restore_data_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FullRestoreAppLaunchHandler::MaybeRestore,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FullRestoreAppLaunchHandler::MaybeRestore() {
  restore_start_time_ = base::TimeTicks::Now();
  ::full_restore::FullRestoreReadHandler::GetInstance()->SetCheckRestoreData(
      profile_->GetPath());

  if (should_launch_browser_ && CanLaunchBrowser()) {
    LaunchBrowser();
    should_launch_browser_ = false;
  }

  VLOG(1) << "Restore apps in " << profile_->GetPath();
  if (FullRestoreArcTaskHandler::GetForProfile(profile_)) {
    FullRestoreArcTaskHandler::GetForProfile(profile_)
        ->arc_app_launch_handler()
        ->RestoreArcApps(this);
  }

  LaunchApps();

  MaybeStartSaveTimer();
}

bool FullRestoreAppLaunchHandler::CanLaunchBrowser() {
  return should_restore_ && restore_data_ &&
         (!restore_data_->HasAppTypeBrowser() || are_web_apps_initialized_);
}

void FullRestoreAppLaunchHandler::LaunchBrowser() {
  // If the browser is not launched before reboot, don't launch browser during
  // the startup phase.
  const auto& launch_list = restore_data_->app_id_to_launch_list();
  if (launch_list.find(extension_misc::kChromeAppId) == launch_list.end())
    return;

  VLOG(1) << "Restore browser for " << profile_->GetPath();
  RecordRestoredAppLaunch(apps::AppTypeName::kChromeBrowser);

  restore_data_->RemoveApp(extension_misc::kChromeAppId);

  if (profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kHideCrashRestoreBubble);
  }

  MaybeStartSaveTimer();

  if (!::full_restore::HasBrowser(profile_->GetPath())) {
    // If there is no normal browsers before reboot, call session restore to
    // restore app type browsers only.
    SessionRestore::RestoreSession(
        profile_, nullptr, SessionRestore::RESTORE_APPS, std::vector<GURL>());
    return;
  }

  // Modify the command line to restore browser sessions.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kRestoreLastSession);

  UserSessionManager::GetInstance()->LaunchBrowser(profile_);
  RecordLaunchBrowserResult();
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

  UserSessionManager::GetInstance()->LaunchBrowser(profile_);

  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  SessionStartupPref session_startup_pref =
      SessionStartupPref::GetStartupPref(prefs);

  // If the system is upgrading from a crash, the app type browser window can be
  // restored, so we don't need to call session restore to restore app type
  // browsers. If the session restore setting is not restore, we don't need to
  // restore app type browser neither.
  if (profile_->GetLastSessionExitType() != Profile::EXIT_CRASHED &&
      !::full_restore::HasAppTypeBrowser(profile_->GetPath()) &&
      session_startup_pref.type == SessionStartupPref::LAST) {
    // Restore the app type browsers only when the web apps are ready.
    SessionRestore::RestoreSession(
        profile_, nullptr, SessionRestore::RESTORE_APPS, std::vector<GURL>());
  }

  UserSessionManager::GetInstance()->MaybeLaunchSettings(profile_);
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
  std::list<SessionServiceEvent> events = GetSessionServiceEvents(profile_);
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
}

void FullRestoreAppLaunchHandler::LogRestoreData() {
  if (!restore_data_ || restore_data_->app_id_to_launch_list().empty()) {
    VLOG(1) << "There is no restore data from " << profile_->GetPath();
    return;
  }

  int arc_app_count = 0;
  int other_app_count = 0;
  for (const auto& it : restore_data_->app_id_to_launch_list()) {
    if (it.first == extension_misc::kChromeAppId || it.second.empty())
      continue;

    if (it.second.begin()->second->event_flag.has_value()) {
      ++arc_app_count;
      continue;
    }

    ++other_app_count;
  }
  VLOG(1) << "There is restore data: Browser("
          << (::full_restore::HasAppTypeBrowser(profile_->GetPath())
                  ? " has app type browser "
                  : " no app type browser")
          << ","
          << (::full_restore::HasBrowser(profile_->GetPath())
                  ? " has normal browser "
                  : " no normal ")
          << ") ARC(" << arc_app_count << ") other apps(" << other_app_count
          << ") in " << profile_->GetPath();
}

void FullRestoreAppLaunchHandler::MaybeStartSaveTimer() {
  if (!should_restore_) {
    // FullRestoreService is responsible to handle all non restore processes.
    return;
  }

  if (!restore_data_ || restore_data_->app_id_to_launch_list().empty()) {
    // If there is no restore data, start the timer.
    ::full_restore::FullRestoreSaveHandler::GetInstance()->AllowSave();
    return;
  }

  if (base::Contains(restore_data_->app_id_to_launch_list(),
                     extension_misc::kChromeAppId)) {
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

ScopedLaunchBrowserForTesting::ScopedLaunchBrowserForTesting() {
  g_launch_browser_for_testing = true;
}

ScopedLaunchBrowserForTesting::~ScopedLaunchBrowserForTesting() {
  g_launch_browser_for_testing = false;
}

}  // namespace full_restore
}  // namespace ash

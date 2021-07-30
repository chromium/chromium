// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_app_launch_handler.h"

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
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/full_restore/arc_app_launch_handler.h"
#include "chrome/browser/chromeos/full_restore/full_restore_arc_task_handler.h"
#include "chrome/browser/chromeos/full_restore/full_restore_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/common/chrome_switches.h"
#include "components/full_restore/full_restore_read_handler.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "extensions/common/constants.h"

namespace chromeos {
namespace full_restore {

namespace {

bool g_launch_browser_for_testing = false;

constexpr char kRestoredAppLaunchHistogramPrefix[] = "Apps.RestoredAppLaunch";

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
          chromeos::switches::kForceLaunchBrowser)) {
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

void FullRestoreAppLaunchHandler::OnAppUpdate(const apps::AppUpdate& update) {
  // If the restore flag `should_restore_` is true, launch the app for
  // restoration.
  if (should_restore_)
    AppLaunchHandler::OnAppUpdate(update);
}

void FullRestoreAppLaunchHandler::OnAppTypeInitialized(
    apps::mojom::AppType app_type) {
  if (app_type != apps::mojom::AppType::kWeb)
    return;

  are_web_apps_initialized_ = true;

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

  // FullRestoreAppLaunchHandler could be created multiple times in browser
  // tests, and used by the desk template. Only when it is created by
  // FullRestoreService, we need to init FullRestoreService.
  if (should_init_service_)
    FullRestoreService::GetForProfile(profile_)->Init();

  if (ProfileHelper::Get()->GetUserByProfile(profile_) ==
      user_manager::UserManager::Get()->GetPrimaryUser()) {
    ::full_restore::FullRestoreSaveHandler::GetInstance()
        ->SetPrimaryProfilePath(profile_->GetPath());

    // In Multi-Profile mode, only set for the primary user. For other users,
    // active profile path is set when switch users.
    ::full_restore::SetActiveProfilePath(profile_->GetPath());
  }

  MaybePostRestore();
}

void FullRestoreAppLaunchHandler::MaybePostRestore() {
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

  if (FullRestoreArcTaskHandler::GetForProfile(profile_)) {
    FullRestoreArcTaskHandler::GetForProfile(profile_)
        ->arc_app_launch_handler()
        ->RestoreArcApps(this);
  }

  LaunchApps();
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

  RecordRestoredAppLaunch(apps::AppTypeName::kChromeBrowser);

  restore_data_->RemoveApp(extension_misc::kChromeAppId);

  if (profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kHideCrashRestoreBubble);
  }

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

ScopedLaunchBrowserForTesting::ScopedLaunchBrowserForTesting() {
  g_launch_browser_for_testing = true;
}

ScopedLaunchBrowserForTesting::~ScopedLaunchBrowserForTesting() {
  g_launch_browser_for_testing = false;
}

}  // namespace full_restore
}  // namespace chromeos

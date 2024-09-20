// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/first_run/first_run.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace ash {
namespace first_run {

namespace {

// Returns true if this user type is probably a human who wants to configure
// their device through the help app. Other user types are robots, guests or
// public accounts.
bool IsRegularUserOrSupervisedChild(user_manager::UserManager* user_manager) {
  switch (user_manager->GetActiveUser()->GetType()) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
      return true;
    default:
      return false;
  }
}

// Getting started module is shown to unmanaged regular and child accounts.
bool ShouldShowGetStarted(Profile* profile,
                          user_manager::UserManager* user_manager) {
  // Child users return true for IsManaged. These are not EDU accounts though,
  // should still see the getting started module.
  if (profile->IsChild())
    return true;
  switch (user_manager->GetActiveUser()->GetType()) {
    case user_manager::UserType::kRegular:
      return !profile->GetProfilePolicyConnector()->IsManaged();
    default:
      return false;
  }
}

// Object of this class waits for system web apps to load. Then it launches the
// help app. The object deletes itself if the app is launched or the profile is
// destroyed.
class AppLauncher final : public ProfileObserver {
 public:
  // App launcher owns itself and will be deleted when the app is launched or
  // the profile is destroyed.
  static void LaunchHelpAfterSWALoad(Profile* profile) {
    DCHECK(ShouldLaunchHelpApp(profile));
    new AppLauncher(profile);
  }
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override { delete this; }

 private:
  explicit AppLauncher(Profile* profile) : profile_(profile) {
    profile->AddObserver(this);
    SystemWebAppManager::Get(profile)->on_apps_synchronized().Post(
        FROM_HERE, base::BindOnce(&AppLauncher::LaunchHelpApp,
                                  weak_factory_.GetWeakPtr()));
  }

  ~AppLauncher() override { this->profile_->RemoveObserver(this); }
  AppLauncher(const AppLauncher&) = delete;
  AppLauncher& operator=(const AppLauncher&) = delete;

  void LaunchHelpApp() {
    ash::SystemAppLaunchParams params;
    params.url = GURL("chrome://help-app?launchSource=first-run");
    params.launch_source = apps::LaunchSource::kFromFirstRun;
    LaunchSystemWebAppAsync(profile_, SystemWebAppType::HELP, params);
    profile_->GetPrefs()->SetBoolean(prefs::kFirstRunTutorialShown, true);
    delete this;
  }
  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<AppLauncher> weak_factory_{this};
};

}  // namespace

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // This preference used to be syncable, change it to non-syncable so new
  // users will always see the welcome app on a new device.
  // See crbug.com/752361
  registry->RegisterBooleanPref(prefs::kFirstRunTutorialShown, false);
  registry->RegisterBooleanPref(prefs::kHelpAppShouldShowGetStarted, false);
  registry->RegisterBooleanPref(prefs::kHelpAppShouldShowParentalControl,
                                false);
  registry->RegisterBooleanPref(prefs::kHelpAppTabletModeDuringOobe, false);
}

bool ShouldLaunchHelpApp(Profile* profile) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  // Even if we don't launch the help app now, define the preferences for what
  // should be shown in the app when it is launched.
  profile->GetPrefs()->SetBoolean(prefs::kHelpAppShouldShowGetStarted,
                                  ShouldShowGetStarted(profile, user_manager));
  profile->GetPrefs()->SetBoolean(prefs::kHelpAppTabletModeDuringOobe,
                                  display::Screen::GetScreen()->InTabletMode());

  if (WizardController::default_controller())
    WizardController::default_controller()->PrepareFirstRunPrefs();

  if (!SystemWebAppManager::Get(profile))
    return false;

  if (!IsRegularUserOrSupervisedChild(user_manager))
    return false;

  if (switches::ShouldSkipOobePostLogin())
    return false;

  if (command_line->HasSwitch(switches::kForceFirstRunUI)) {
    return true;
  }

  if (command_line->HasSwitch(switches::kDisableFirstRunUI)) {
    return false;
  }

  if (display::Screen::GetScreen()->InTabletMode()) {
    return false;
  }

  if (command_line->HasSwitch(::switches::kTestType))
    return false;

  if (!user_manager->IsCurrentUserNew())
    return false;

  if (profile->GetPrefs()->GetBoolean(prefs::kFirstRunTutorialShown))
    return false;

  if (user_manager->IsCurrentUserNonCryptohomeDataEphemeral())
    return false;

  // Child accounts show up as managed, so check this first.
  if (profile->IsChild())
    return true;

  if (profile->GetProfilePolicyConnector()->IsManaged())
    return false;

  return true;
}

void LaunchHelpApp(Profile* profile) {
  AppLauncher::LaunchHelpAfterSWALoad(profile);
}

}  // namespace first_run
}  // namespace ash

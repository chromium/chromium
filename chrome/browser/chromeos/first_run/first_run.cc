// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/first_run.h"

#include "ash/public/cpp/tablet_mode.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/first_run/first_run_controller.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
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
#include "ui/gfx/geometry/rect.h"

namespace chromeos {
namespace first_run {

namespace {

void LaunchDialogForProfile(Profile* profile) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  if (!registry)
    return;

  const extensions::Extension* extension =
      registry->GetExtensionById(extension_misc::kFirstRunDialogId,
                                 extensions::ExtensionRegistry::ENABLED);
  if (!extension)
    return;

  apps::LaunchService::Get(profile)->OpenApplication(apps::AppLaunchParams(
      extension->id(), apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceChromeInternal));
  profile->GetPrefs()->SetBoolean(prefs::kFirstRunTutorialShown, true);
}

void TryLaunchFirstRunDialog(Profile* profile) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (chromeos::switches::ShouldSkipOobePostLogin())
    return;

  if (command_line->HasSwitch(switches::kForceFirstRunUI)) {
    LaunchDialogForProfile(profile);
    return;
  }

  // ash::TabletMode does not exist in some tests.
  if (ash::TabletMode::Get() && ash::TabletMode::Get()->InTabletMode())
    return;

  if (profile->GetProfilePolicyConnector()->IsManaged())
    return;

  if (command_line->HasSwitch(::switches::kTestType))
    return;

  if (!user_manager::UserManager::Get()->IsCurrentUserNew())
    return;

  if (profile->GetPrefs()->GetBoolean(prefs::kFirstRunTutorialShown))
    return;

  bool is_pref_synced =
      PrefServiceSyncableFromProfile(profile)->IsPrioritySyncing();
  bool is_user_ephemeral = user_manager::UserManager::Get()
                               ->IsCurrentUserNonCryptohomeDataEphemeral();
  if (!is_pref_synced && is_user_ephemeral)
    return;

  LaunchDialogForProfile(profile);
}

// Object of this class waits for session start. Then it launches or not
// launches first-run dialog depending on user prefs and flags. Than object
// deletes itself.
class DialogLauncher : public session_manager::SessionManagerObserver {
 public:
  DialogLauncher() {
    session_manager::SessionManager::Get()->AddObserver(this);
  }

  ~DialogLauncher() override {
    session_manager::SessionManager::Get()->RemoveObserver(this);
  }

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override {
    auto* profile = ProfileHelper::Get()->GetProfileByUser(
        user_manager::UserManager::Get()->GetActiveUser());
    TryLaunchFirstRunDialog(profile);
    delete this;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DialogLauncher);
};

}  // namespace

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // This preference used to be syncable, change it to non-syncable so new
  // users will always see the welcome app on a new device.
  // See crbug.com/752361
  registry->RegisterBooleanPref(prefs::kFirstRunTutorialShown, false);
}

void MaybeLaunchDialogAfterSessionStart() {
  new DialogLauncher();
}

void LaunchTutorial() {
  UMA_HISTOGRAM_BOOLEAN("CrosFirstRun.TutorialLaunched", true);
  FirstRunController::Start();
}

}  // namespace first_run
}  // namespace chromeos

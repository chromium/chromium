// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/apps_helper.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_app_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest.h"

using sync_datatype_helper::test;

namespace {

std::string CreateFakeAppName(int index) {
  return "fakeapp" + base::NumberToString(index);
}

void FlushPendingOperations(
    std::vector<raw_ptr<Profile, VectorExperimental>> profiles) {
  for (Profile* profile : profiles) {
    web_app::WebAppProvider::GetForTest(profile)
        ->command_manager()
        .AwaitAllCommandsCompleteForTesting();

    // First, wait for all installations to complete.

    base::flat_set<webapps::AppId> apps_to_be_installed =
        web_app::WebAppProvider::GetForTest(profile)
            ->registrar_unsafe()
            .GetAppsFromSyncAndPendingInstallation();

    if (!apps_to_be_installed.empty()) {
      // Because we don't know whether these have been installed yet or if we
      // are waiting for installation with hooks, wait on either.
      base::RunLoop loop;
      auto install_listener_callback =
          base::BindLambdaForTesting([&](const webapps::AppId& app_id) {
            apps_to_be_installed.erase(app_id);
            if (apps_to_be_installed.empty())
              loop.Quit();
          });

      web_app::WebAppInstallManagerObserverAdapter install_adapter(profile);
      install_adapter.SetWebAppInstalledDelegate(install_listener_callback);
      install_adapter.SetWebAppInstalledWithOsHooksDelegate(
          install_listener_callback);
      loop.Run();
    }

    // Next, wait for uninstalls. These are easier because they don't have two
    // stages.
    web_app::WebAppProvider::GetForTest(profile)
        ->command_manager()
        .AwaitAllCommandsCompleteForTesting();
  }
}

}  // namespace

namespace apps_helper {

bool HasSameApps(Profile* profile1, Profile* profile2) {
  return SyncAppHelper::GetInstance()->AppStatesMatch(profile1, profile2);
}

bool AllProfilesHaveSameApps() {
  const std::vector<raw_ptr<Profile, VectorExperimental>>& profiles =
      test()->GetAllProfiles();
  for (Profile* profile : profiles) {
    if (profile != profiles.front() &&
        !HasSameApps(profiles.front(), profile)) {
      DVLOG(1) << "Profiles apps do not match.";
      return false;
    }
  }
  return true;
}

std::string InstallHostedApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->InstallExtension(
      profile, CreateFakeAppName(index), extensions::Manifest::TYPE_HOSTED_APP);
}

std::string InstallPlatformApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->InstallExtension(
      profile, CreateFakeAppName(index),
      extensions::Manifest::TYPE_PLATFORM_APP);
}

std::string InstallHostedAppForAllProfiles(int index) {
  std::string extension_id;
  for (Profile* profile : test()->GetAllProfiles()) {
    extension_id = InstallHostedApp(profile, index);
  }
  return extension_id;
}

void UninstallApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->UninstallExtension(
      profile, CreateFakeAppName(index));
}

void EnableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->EnableExtension(
      profile, CreateFakeAppName(index));
}

void DisableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->DisableExtension(
      profile, CreateFakeAppName(index));
}

bool IsAppEnabled(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IsExtensionEnabled(
      profile, CreateFakeAppName(index));
}

void IncognitoEnableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoEnableExtension(
      profile, CreateFakeAppName(index));
}

void IncognitoDisableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoDisableExtension(
      profile, CreateFakeAppName(index));
}

bool IsIncognitoEnabled(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IsIncognitoEnabled(
      profile, CreateFakeAppName(index));
}

void InstallAppsPendingForSync(Profile* profile) {
  SyncExtensionHelper::GetInstance()->InstallExtensionsPendingForSync(profile);
}

syncer::StringOrdinal GetPageOrdinalForApp(Profile* profile, int app_index) {
  return SyncAppHelper::GetInstance()->GetPageOrdinalForApp(
      profile, CreateFakeAppName(app_index));
}

void SetPageOrdinalForApp(Profile* profile,
                          int app_index,
                          const syncer::StringOrdinal& page_ordinal) {
  SyncAppHelper::GetInstance()->SetPageOrdinalForApp(
      profile, CreateFakeAppName(app_index), page_ordinal);
}

syncer::StringOrdinal GetAppLaunchOrdinalForApp(Profile* profile,
                                                int app_index) {
  return SyncAppHelper::GetInstance()->GetAppLaunchOrdinalForApp(
      profile, CreateFakeAppName(app_index));
}

void SetAppLaunchOrdinalForApp(
    Profile* profile,
    int app_index,
    const syncer::StringOrdinal& app_launch_ordinal) {
  SyncAppHelper::GetInstance()->SetAppLaunchOrdinalForApp(
      profile, CreateFakeAppName(app_index), app_launch_ordinal);
}

void CopyNTPOrdinals(Profile* source, Profile* destination, int index) {
  SetPageOrdinalForApp(destination, index, GetPageOrdinalForApp(source, index));
  SetAppLaunchOrdinalForApp(destination, index,
                            GetAppLaunchOrdinalForApp(source, index));
}

void FixNTPOrdinalCollisions(Profile* profile) {
  SyncAppHelper::GetInstance()->FixNTPOrdinalCollisions(profile);
}

bool AwaitWebAppQuiescence(
    std::vector<raw_ptr<Profile, VectorExperimental>> profiles) {
  FlushPendingOperations(profiles);

  // If sync is off, then `AwaitQuiescence()` will crash. This code can be
  // removed once https://crbug.com/1330792 is fixed.
  if (sync_datatype_helper::test()) {
    SyncTest* test = sync_datatype_helper::test();
    bool is_sync_on = true;
    for (SyncServiceImplHarness* client : test->GetSyncClients()) {
      is_sync_on = is_sync_on && client->service()->IsSyncFeatureActive();
    }
    if (is_sync_on) {
      if (!test->AwaitQuiescence())
        return false;
      FlushPendingOperations(profiles);
    }
  }

  for (Profile* profile : profiles) {
    // Only checks that there is no app in sync install state in the registry.
    // Do not use |GetEnqueuedInstallAppIdsForTesting| because the task only
    // gets removed from the queue on WebAppInstallTask::OnOsHooksCreated that
    // happens asynchronously after the observer gets OnWebAppInstalled. And
    // some installs might not have OS hooks installed but they will be in the
    // registry.
    auto* provider = web_app::WebAppProvider::GetForTest(profile);
    std::vector<webapps::AppId> sync_apps_pending_install =
        provider->registrar_unsafe().GetAppsFromSyncAndPendingInstallation();
    if (!sync_apps_pending_install.empty()) {
      LOG(ERROR) << "Apps from sync are still pending installation: "
                 << sync_apps_pending_install.size();
      return false;
    }

    std::vector<webapps::AppId> apps_in_uninstall =
        provider->registrar_unsafe().GetAppsPendingUninstall();
    if (!apps_in_uninstall.empty()) {
      LOG(ERROR) << "App uninstalls are still pending: "
                 << apps_in_uninstall.size();
      return false;
    }
  }
  return true;
}

webapps::AppId InstallWebApp(Profile* profile,
                             std::unique_ptr<web_app::WebAppInstallInfo> info) {
  return web_app::test::InstallWebApp(
      profile, std::move(info),
      /*overwrite_existing_manifest_fields=*/true);
}

}  // namespace apps_helper

AppsStatusChangeChecker::AppsStatusChangeChecker()
    : profiles_(test()->GetAllProfiles()) {
  DCHECK_GE(profiles_.size(), 2U);

  for (Profile* profile : profiles_) {
    InstallSyncedApps(profile);

    // Fake the installation of synced apps from the web store.
    CHECK(extensions::ExtensionSystem::Get(profile)
              ->extension_service()
              ->updater());
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->updater()
        ->SetUpdatingStartedCallbackForTesting(base::BindLambdaForTesting(
            [self = weak_ptr_factory_.GetWeakPtr(), profile]() {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&AppsStatusChangeChecker::InstallSyncedApps,
                                 self, base::Unretained(profile)));
            }));

    // Register as an observer of ExtensionsRegistry to receive notifications of
    // big events, like installs and uninstalls.
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    registry->AddObserver(this);

    // Register for ExtensionPrefs events, too, so we can get notifications
    // about
    // smaller but still syncable events, like launch type changes.
    extensions::ExtensionPrefs* prefs =
        extensions::ExtensionPrefs::Get(profile);
    prefs->AddObserver(this);

    install_tracker_observation_.AddObservation(
        extensions::InstallTracker::Get(profile));
  }
}

AppsStatusChangeChecker::~AppsStatusChangeChecker() {
  for (Profile* profile : profiles_) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    registry->RemoveObserver(this);
    extensions::ExtensionPrefs* prefs =
        extensions::ExtensionPrefs::Get(profile);
    prefs->RemoveObserver(this);
  }
}

void AppsStatusChangeChecker::OnExtensionLoaded(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  CheckExitCondition();
}

void AppsStatusChangeChecker::OnExtensionUnloaded(
    content::BrowserContext* context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  CheckExitCondition();
}

void AppsStatusChangeChecker::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  CheckExitCondition();
}

void AppsStatusChangeChecker::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  CheckExitCondition();
}

void AppsStatusChangeChecker::OnExtensionDisableReasonsChanged(
    const std::string& extension_id,
    int disabled_reasons) {
  CheckExitCondition();
}

void AppsStatusChangeChecker::OnExtensionRegistered(
    const std::string& extension_id,
    const base::Time& install_time,
    bool is_enabled) {
  CheckExitCondition();
}

void AppsStatusChangeChecker::OnExtensionPrefsLoaded(
    const std::string& extension_id,
    const extensions::ExtensionPrefs* prefs) {
  CheckExitCondition();
}

void AppsStatusChangeChecker::OnExtensionPrefsDeleted(
    const std::string& extension_id) {
  CheckExitCondition();
}

void AppsStatusChangeChecker::OnExtensionStateChanged(
    const std::string& extension_id,
    bool state) {
  CheckExitCondition();
}

void AppsStatusChangeChecker::OnAppsReordered(
    content::BrowserContext* context,
    const std::optional<std::string>& extension_id) {
  CheckExitCondition();
}

void AppsStatusChangeChecker::InstallSyncedApps(Profile* profile) {
  // Installs apps too.
  SyncExtensionHelper::GetInstance()->InstallExtensionsPendingForSync(profile);
}

AppsMatchChecker::AppsMatchChecker() = default;

bool AppsMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for apps to match";

  auto it = profiles_.begin();
  Profile* profile0 = *it;
  ++it;
  for (; it != profiles_.end(); ++it) {
    if (!SyncAppHelper::GetInstance()->AppStatesMatch(profile0, *it)) {
      return false;
    }
  }
  return true;
}

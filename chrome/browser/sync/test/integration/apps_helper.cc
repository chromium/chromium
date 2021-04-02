// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/apps_helper.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_app_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_installer.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest.h"

using sync_datatype_helper::test;

namespace {

std::string CreateFakeAppName(int index) {
  return "fakeapp" + base::NumberToString(index);
}

std::unique_ptr<web_app::WebAppInstallObserver>
SetupSyncInstallObserverForProfile(Profile* profile) {
  auto apps_to_be_sync_installed = web_app::WebAppProvider::Get(profile)
                                       ->install_manager()
                                       .GetEnqueuedInstallAppIdsForTesting();

  if (apps_to_be_sync_installed.empty()) {
    return nullptr;
  }
  return web_app::WebAppInstallObserver::CreateInstallListener(
      profile, apps_to_be_sync_installed);
}

std::unique_ptr<web_app::WebAppInstallObserver>
SetupSyncUninstallObserverForProfile(Profile* profile) {
  std::set<web_app::AppId> apps_in_sync_uninstall =
      web_app::WebAppProvider::Get(profile)
          ->registry_controller()
          .AsWebAppSyncBridge()
          ->GetAppsInSyncUninstallForTest();

  if (apps_in_sync_uninstall.empty()) {
    return nullptr;
  }
  return web_app::WebAppInstallObserver::CreateUninstallListener(
      profile, apps_in_sync_uninstall);
}

}  // namespace

namespace apps_helper {

bool HasSameApps(Profile* profile1, Profile* profile2) {
  return SyncAppHelper::GetInstance()->AppStatesMatch(profile1, profile2);
}

bool AllProfilesHaveSameApps() {
  const auto& profiles = test()->GetAllProfiles();
  for (auto* profile : profiles) {
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
      profile,
      CreateFakeAppName(index),
      extensions::Manifest::TYPE_HOSTED_APP);
}

std::string InstallPlatformApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->InstallExtension(
      profile,
      CreateFakeAppName(index),
      extensions::Manifest::TYPE_PLATFORM_APP);
}

std::string InstallHostedAppForAllProfiles(int index) {
  std::string extension_id;
  for (auto* profile : test()->GetAllProfiles()) {
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
  WaitForAppService(profile);
}

void WaitForAppService(Profile* profile) {
  // The App Service is a Mojo service, and Mojo calls are asynchronous
  // (because they are potentially IPC calls). When the tests install and
  // uninstall apps, they may need to pump the run loop so that those async
  // calls settle.
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->FlushMojoCallsForTesting();
}

syncer::StringOrdinal GetPageOrdinalForApp(Profile* profile,
                                           int app_index) {
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
  SetAppLaunchOrdinalForApp(
      destination, index, GetAppLaunchOrdinalForApp(source, index));
}

void FixNTPOrdinalCollisions(Profile* profile) {
  SyncAppHelper::GetInstance()->FixNTPOrdinalCollisions(profile);
}

void AwaitWebAppQuiescence(std::vector<Profile*> profiles) {
  for (auto* profile : profiles) {
    auto install_observer = SetupSyncInstallObserverForProfile(profile);
    // This actually waits for all observed apps to be installed.
    if (install_observer)
      install_observer->AwaitNextInstall();

    auto uninstall_observer = SetupSyncUninstallObserverForProfile(profile);
    // This actually waits for all observed apps to be installed.
    if (uninstall_observer) {
      web_app::WebAppInstallObserver::AwaitNextUninstall(
          uninstall_observer.get());
    }
  }

  for (auto* profile : profiles) {
    // Only checks that there is no app in sync install state in the registry.
    // Do not use |GetEnqueuedInstallAppIdsForTesting| because the task only
    // gets removed from the queue on WebAppInstallTask::OnOsHooksCreated that
    // happens asynchronously after the observer gets OnWebAppInstalled. And
    // some installs might not have OS hooks installed but they will be in the
    // registry.
    ASSERT_TRUE(web_app::WebAppProvider::Get(profile)
                    ->registrar()
                    .AsWebAppRegistrar()
                    ->GetAppsInSyncInstall()
                    .empty());

    std::set<web_app::AppId> apps_in_sync_uninstall =
        web_app::WebAppProvider::Get(profile)
            ->registry_controller()
            .AsWebAppSyncBridge()
            ->GetAppsInSyncUninstallForTest();
    ASSERT_TRUE(apps_in_sync_uninstall.empty());
  }
}

web_app::AppId InstallWebApp(Profile* profile, const WebApplicationInfo& info) {
  DCHECK(info.start_url.is_valid());
  base::RunLoop run_loop;
  web_app::AppId app_id;
  auto* provider = web_app::WebAppProvider::Get(profile);
  provider->install_manager().InstallWebAppFromInfo(
      std::make_unique<WebApplicationInfo>(info),
      web_app::ForInstallableSite::kYes,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      base::BindLambdaForTesting(
          [&run_loop, &app_id](const web_app::AppId& new_app_id,
                               web_app::InstallResultCode code) {
            DCHECK_EQ(code, web_app::InstallResultCode::kSuccessNewInstall);
            app_id = new_app_id;
            run_loop.Quit();
          }));
  run_loop.Run();

  const web_app::AppRegistrar& registrar = provider->registrar();
  DCHECK_EQ(base::UTF8ToUTF16(registrar.GetAppShortName(app_id)), info.title);
  DCHECK_EQ(registrar.GetAppStartUrl(app_id), info.start_url);

  return app_id;
}

}  // namespace apps_helper

AppsMatchChecker::AppsMatchChecker() : profiles_(test()->GetAllProfiles()) {
  DCHECK_GE(profiles_.size(), 2U);

  for (Profile* profile : profiles_) {
    // Begin mocking the installation of synced extensions from the web store.
    synced_extension_installers_.push_back(
        std::make_unique<SyncedExtensionInstaller>(profile));

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
  }

  registrar_.Add(this, chrome::NOTIFICATION_APP_LAUNCHER_REORDERED,
                 content::NotificationService::AllSources());
}

AppsMatchChecker::~AppsMatchChecker() {
  for (Profile* profile : profiles_) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    registry->RemoveObserver(this);
    extensions::ExtensionPrefs* prefs =
        extensions::ExtensionPrefs::Get(profile);
    prefs->RemoveObserver(this);
  }

  registrar_.Remove(this, chrome::NOTIFICATION_APP_LAUNCHER_REORDERED,
                    content::NotificationService::AllSources());
}

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

void AppsMatchChecker::OnExtensionLoaded(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionUnloaded(
    content::BrowserContext* context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionDisableReasonsChanged(
    const std::string& extension_id,
    int disabled_reasons) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionRegistered(const std::string& extension_id,
                                             const base::Time& install_time,
                                             bool is_enabled) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionPrefsLoaded(
    const std::string& extension_id,
    const extensions::ExtensionPrefs* prefs) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionPrefsDeleted(
    const std::string& extension_id) {
  CheckExitCondition();
}

void AppsMatchChecker::OnExtensionStateChanged(const std::string& extension_id,
                                               bool state) {
  CheckExitCondition();
}

void AppsMatchChecker::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_LAUNCHER_REORDERED, type);
  CheckExitCondition();
}

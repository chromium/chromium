// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/borealis_apps.h"

#include <optional>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/publishers/guest_os_apps.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_app_uninstaller.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

struct PermissionInfo {
  apps::PermissionType permission;
  const char* pref_name;
};

constexpr PermissionInfo permission_infos[] = {
    {apps::PermissionType::kMicrophone, borealis::prefs::kBorealisMicAllowed},
};

const char* PermissionToPrefName(apps::PermissionType permission) {
  for (const PermissionInfo& info : permission_infos) {
    if (info.permission == permission) {
      return info.pref_name;
    }
  }
  return nullptr;
}

// Helper method to set up an apps visibility in all the major UI surfaces.
void SetAppVisibility(apps::App& app, bool visible) {
  app.recommendable = visible;
  app.searchable = visible;
  app.show_in_launcher = visible;
  app.show_in_shelf = visible;
  app.show_in_search = visible;
  app.show_in_management = visible;
  app.handles_intents = visible;
}

apps::Permissions CreatePermissions(Profile* profile) {
  apps::Permissions permissions;
  for (const PermissionInfo& info : permission_infos) {
    permissions.push_back(std::make_unique<apps::Permission>(
        info.permission, profile->GetPrefs()->GetBoolean(info.pref_name),
        /*is_managed=*/false));
  }
  return permissions;
}

}  // namespace

namespace apps {

BorealisApps::BorealisApps(AppServiceProxy* proxy) : GuestOSApps(proxy) {
  anonymous_app_observation_.Observe(
      &borealis::BorealisServiceFactory::GetForProfile(profile())
           ->WindowManager());

  pref_registrar_.Init(profile()->GetPrefs());

  for (const PermissionInfo& info : permission_infos) {
    pref_registrar_.Add(
        info.pref_name,
        base::BindRepeating(&apps::BorealisApps::OnPermissionChanged,
                            weak_factory_.GetWeakPtr()));
  }

  pref_registrar_.Add(borealis::prefs::kBorealisInstalledOnDevice,
                      base::BindRepeating(&BorealisApps::RefreshSpecialApps,
                                          weak_factory_.GetWeakPtr()));

  // TODO(b/170264723): When uninstalling borealis is completed, ensure that we
  // remove the apps from the apps service.
}

BorealisApps::~BorealisApps() {
  pref_registrar_.RemoveAll();
}

void BorealisApps::CallWithBorealisAllowed(
    base::OnceCallback<void(bool)> callback) {
  borealis::BorealisServiceFactory::GetForProfile(profile())
      ->Features()
      .IsAllowed(base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             borealis::BorealisFeatures::AllowStatus allow_status) {
            std::move(callback).Run(
                allow_status ==
                borealis::BorealisFeatures::AllowStatus::kAllowed);
          },
          std::move(callback)));
}

void BorealisApps::SetUpSpecialApps(bool allowed) {
  // The special apps are only shown if borealis isn't installed and it can be.
  bool installed = borealis::BorealisServiceFactory::GetForProfile(profile())
                       ->Features()
                       .IsEnabled();
  bool shown = allowed && !installed;

  // An app for borealis' installer. This app is not shown to users via
  // launcher/management, and is only visible on the shelf.
  auto installer_app = apps::AppPublisher::MakeApp(
      apps::AppType::kBorealis, borealis::kInstallerAppId,
      shown ? apps::Readiness::kReady : apps::Readiness::kDisabledByPolicy,
      l10n_util::GetStringUTF8(IDS_BOREALIS_INSTALLER_APP_NAME),
      apps::InstallReason::kDefault, apps::InstallSource::kSystem);
  SetAppVisibility(*installer_app, shown);
  installer_app->icon_key = apps::IconKey(IDR_LOGO_BOREALIS_STEAM_PENDING_192,
                                          apps::IconEffects::kNone);
  installer_app->show_in_launcher = false;
  installer_app->show_in_management = false;
  installer_app->show_in_search = false;
  installer_app->allow_uninstall = false;
  installer_app->allow_close = true;
  AppPublisher::Publish(std::move(installer_app));

  // A "steam" app, which is shown in launcher searches. This app is essentially
  // a front for the installer, but we need it for two reasons:
  //  - The "real" steam app comes with the VM and so we won't have it before
  //    installation.
  //  - We want users to be able to search for steam and see the correct icon,
  //    but we want to visually distinguish that from the installer app above.
  auto initial_steam_app = apps::AppPublisher::MakeApp(
      apps::AppType::kBorealis, borealis::kLauncherSearchAppId,
      shown ? apps::Readiness::kReady : apps::Readiness::kDisabledByPolicy,
      l10n_util::GetStringUTF8(IDS_BOREALIS_INSTALLER_APP_NAME),
      apps::InstallReason::kDefault, apps::InstallSource::kSystem);
  SetAppVisibility(*initial_steam_app, shown);
  initial_steam_app->icon_key =
      apps::IconKey(IDR_LOGO_BOREALIS_STEAM_192, apps::IconEffects::kNone);
  initial_steam_app->show_in_launcher = false;
  initial_steam_app->show_in_management = false;
  initial_steam_app->allow_uninstall = false;
  initial_steam_app->allow_close = true;
  AppPublisher::Publish(std::move(initial_steam_app));
}

bool BorealisApps::CouldBeAllowed() const {
  // Borealis's permission check is actually dynamic, so instead of performing
  // it here we just pretend that borealis is always allowed.
  //
  // For Borealis this is useful, we still want to allow uninstall when
  // disallowed (to clean up the disk) but that requires us to provide an app
  // which the user can right click on and uninstall.
  return true;
}

apps::AppType BorealisApps::AppType() const {
  return apps::AppType::kBorealis;
}

guest_os::VmType BorealisApps::VmType() const {
  return guest_os::VmType::BOREALIS;
}

void BorealisApps::Initialize() {
  GuestOSApps::Initialize();

  CallWithBorealisAllowed(base::BindOnce(&BorealisApps::SetUpSpecialApps,
                                         weak_factory_.GetWeakPtr()));
}

void BorealisApps::CreateAppOverrides(
    const guest_os::GuestOsRegistryService::Registration& registration,
    App* app) {
  // The special apps are not GuestOs apps, they don't have a registration and
  // can't be converted.
  DCHECK_NE(registration.app_id(), borealis::kInstallerAppId);
  DCHECK_NE(registration.app_id(), borealis::kLauncherSearchAppId);

  // Borealis apps don't handle intents (like "open with").
  app->handles_intents = false;
  // Borealis apps are normal apps per apps-management.
  app->show_in_management = true;
  // Borealis supports uninstall per-app
  app->allow_uninstall = true;

  // Hide some known spurious "apps" from the user.
  if (borealis::ShouldHideIrrelevantApp(registration)) {
    SetAppVisibility(*app, false);
  }

  // Special handling for the steam client itself.
  if (registration.app_id() == borealis::kClientAppId) {
    app->permissions = CreatePermissions(profile());
  } else {
    // Identify games to App Service by PackageId.
    // Steam games have PackageIds like "steam:123", where 123 is the Steam Game
    // ID.
    std::optional<int> app_id = borealis::ParseSteamGameId(registration.Exec());
    if (app_id) {
      app->installer_package_id = PackageId(
          PackageType::kBorealis, base::NumberToString(app_id.value()));
    }
  }
}

int BorealisApps::DefaultIconResourceId() const {
  return IDR_LOGO_BOREALIS_DEFAULT_192;
}

void BorealisApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          LaunchSource launch_source,
                          WindowInfoPtr window_info) {
  LaunchAppWithIntent(app_id, event_flags, /*intent=*/nullptr, launch_source,
                      std::move(window_info), base::DoNothing());
}

void BorealisApps::LaunchAppWithIntent(const std::string& app_id,
                                       int32_t event_flags,
                                       IntentPtr intent,
                                       LaunchSource launch_source,
                                       WindowInfoPtr window_info,
                                       LaunchCallback callback) {
  borealis::BorealisServiceFactory::GetForProfile(profile())
      ->AppLauncher()
      .Launch(app_id, borealis::BorealisLaunchSource::kSteamInstallerApp,
              base::DoNothing());
}

void BorealisApps::SetPermission(const std::string& app_id,
                                 PermissionPtr permission_ptr) {
  auto permission = permission_ptr->permission_type;
  const char* pref_name = PermissionToPrefName(permission);
  if (!pref_name) {
    return;
  }
  profile()->GetPrefs()->SetBoolean(pref_name,
                                    permission_ptr->IsPermissionEnabled());
}

void BorealisApps::Uninstall(const std::string& app_id,
                             UninstallSource uninstall_source,
                             bool clear_site_data,
                             bool report_abuse) {
  borealis::BorealisServiceFactory::GetForProfile(profile())
      ->AppUninstaller()
      .Uninstall(app_id, base::DoNothing());
}

void BorealisApps::GetMenuModel(const std::string& app_id,
                                MenuType menu_type,
                                int64_t display_id,
                                base::OnceCallback<void(MenuItems)> callback) {
  MenuItems menu_items;

  // Apps should only be uninstallable if we can run the VM, but the vm itself
  // should always be uninstallable.
  if (borealis::BorealisServiceFactory::GetForProfile(profile())
          ->Features()
          .IsEnabled() ||
      app_id == borealis::kClientAppId) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile())) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, menu_items);
  }

  // TODO(b/162562622): Menu models for borealis apps.

  std::move(callback).Run(std::move(menu_items));
}

void BorealisApps::OnPermissionChanged() {
  auto app = std::make_unique<App>(AppType::kBorealis, borealis::kClientAppId);
  app->permissions = CreatePermissions(profile());
  AppPublisher::Publish(std::move(app));
}

void BorealisApps::RefreshSpecialApps() {
  CallWithBorealisAllowed(base::BindOnce(&BorealisApps::SetUpSpecialApps,
                                         weak_factory_.GetWeakPtr()));
}

void BorealisApps::OnAnonymousAppAdded(const std::string& shelf_app_id,
                                       const std::string& shelf_app_name) {
  auto app = AppPublisher::MakeApp(
      AppType::kBorealis, shelf_app_id, Readiness::kReady, shelf_app_name,
      InstallReason::kUser, InstallSource::kUnknown);
  SetAppVisibility(*app, /*visible=*/false);

  app->icon_key = IconKey(IDR_LOGO_BOREALIS_DEFAULT_192, IconEffects::kNone);

  AppPublisher::Publish(std::move(app));
}

void BorealisApps::OnAnonymousAppRemoved(const std::string& shelf_app_id) {
  // First uninstall the anonymous app, then remove it.
  for (auto readiness : {Readiness::kUninstalledByUser, Readiness::kRemoved}) {
    auto app = std::make_unique<App>(AppType::kBorealis, shelf_app_id);
    app->readiness = readiness;
    AppPublisher::Publish(std::move(app));
  }
}

void BorealisApps::OnWindowManagerDeleted(
    borealis::BorealisWindowManager* window_manager) {
  anonymous_app_observation_.Reset();
}

}  // namespace apps

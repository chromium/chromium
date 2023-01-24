// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/borealis_apps.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_app_uninstaller.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
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
void InitializeApp(apps::App& app, bool visible) {
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
        info.permission,
        std::make_unique<apps::PermissionValue>(
            profile->GetPrefs()->GetBoolean(info.pref_name)),
        /*is_managed=*/false));
  }
  return permissions;
}

}  // namespace

namespace apps {

BorealisApps::BorealisApps(AppServiceProxy* proxy)
    : AppPublisher(proxy), profile_(proxy->profile()) {
  Registry()->AddObserver(this);

  anonymous_app_observation_.Observe(
      &borealis::BorealisService::GetForProfile(profile_)->WindowManager());

  pref_registrar_.Init(profile_->GetPrefs());

  for (const PermissionInfo& info : permission_infos) {
    pref_registrar_.Add(
        info.pref_name,
        base::BindRepeating(&apps::BorealisApps::OnPermissionChanged,
                            weak_factory_.GetWeakPtr()));
  }

  pref_registrar_.Add(borealis::prefs::kBorealisInstalledOnDevice,
                      base::BindRepeating(&BorealisApps::RefreshSpecialApps,
                                          weak_factory_.GetWeakPtr()));
  pref_registrar_.Add(borealis::prefs::kBorealisVmTokenHash,
                      base::BindRepeating(&BorealisApps::RefreshSpecialApps,
                                          weak_factory_.GetWeakPtr()));

  // TODO(b/170264723): When uninstalling borealis is completed, ensure that we
  // remove the apps from the apps service.
}

BorealisApps::~BorealisApps() {
  Registry()->RemoveObserver(this);
  pref_registrar_.RemoveAll();
}

void BorealisApps::CallWithBorealisAllowed(
    base::OnceCallback<void(bool)> callback) {
  borealis::BorealisService::GetForProfile(profile_)->Features().IsAllowed(
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             borealis::BorealisFeatures::AllowStatus allow_status) {
            bool allowed = allow_status ==
                           borealis::BorealisFeatures::AllowStatus::kAllowed;
            if (!allowed) {
              LOG(WARNING) << "Borealis not allowed: "
                           << static_cast<int>(allow_status);
            }
            std::move(callback).Run(allowed);
          },
          std::move(callback)));
}

void BorealisApps::SetUpSpecialApps(bool allowed) {
  // The special apps are only shown if borealis isn't installed and it can be.
  bool installed = borealis::BorealisService::GetForProfile(profile_)
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
  InitializeApp(*installer_app, shown);
  installer_app->icon_key =
      apps::IconKey(apps::IconKey::kDoesNotChangeOverTime,
                    IDR_LOGO_BOREALIS_DEFAULT_192, apps::IconEffects::kNone);
  installer_app->show_in_launcher = false;
  installer_app->show_in_management = false;
  installer_app->show_in_search = false;
  installer_app->allow_uninstall = false;
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
      l10n_util::GetStringUTF8(IDS_BOREALIS_APP_NAME),
      apps::InstallReason::kDefault, apps::InstallSource::kSystem);
  InitializeApp(*initial_steam_app, shown);
  initial_steam_app->icon_key =
      apps::IconKey(apps::IconKey::kDoesNotChangeOverTime,
                    IDR_LOGO_BOREALIS_STEAM_192, apps::IconEffects::kNone);
  initial_steam_app->show_in_launcher = false;
  initial_steam_app->show_in_management = false;
  initial_steam_app->allow_uninstall = false;
  AppPublisher::Publish(std::move(initial_steam_app));

  // TODO(crbug.com/1253250): Add other fields for the App struct.
}

guest_os::GuestOsRegistryService* BorealisApps::Registry() {
  guest_os::GuestOsRegistryService* registry =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_);
  // The GuestOsRegistryService is a dependant of the apps service itself, so it
  // is not possible for the apps service to be in a valid state while this is
  // null.
  DCHECK(registry);
  return registry;
}

AppPtr BorealisApps::CreateApp(
    const guest_os::GuestOsRegistryService::Registration& registration,
    bool generate_new_icon_key) {
  // We must only convert borealis apps.
  DCHECK_EQ(registration.VmType(), guest_os::VmType::BOREALIS);

  // The special apps are not GuestOs apps, they don't have a registration and
  // can't be converted.
  DCHECK_NE(registration.app_id(), borealis::kInstallerAppId);
  DCHECK_NE(registration.app_id(), borealis::kLauncherSearchAppId);

  bool shown = !registration.NoDisplay();
  auto app = AppPublisher::MakeApp(
      AppType::kBorealis, registration.app_id(),
      shown ? apps::Readiness::kReady : apps::Readiness::kDisabledByPolicy,
      registration.Name(), InstallReason::kUser, InstallSource::kUnknown);
  InitializeApp(*app, shown);

  const std::string& executable_file_name = registration.ExecutableFileName();
  if (!executable_file_name.empty()) {
    app->additional_search_terms.push_back(executable_file_name);
  }

  for (const std::string& keyword : registration.Keywords()) {
    app->additional_search_terms.push_back(keyword);
  }

  if (generate_new_icon_key) {
    app->icon_key = std::move(
        *icon_key_factory_.CreateIconKey(IconEffects::kCrOsStandardIcon));
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  if (registration.app_id() == borealis::kClientAppId) {
    app->permissions = CreatePermissions(profile_);
  }

  // TODO(crbug.com/1253250): Add other fields for the App struct.
  return app;
}

void BorealisApps::Initialize() {
  RegisterPublisher(AppType::kBorealis);

  std::vector<AppPtr> apps;
  for (const auto& pair :
       Registry()->GetRegisteredApps(guest_os::VmType::BOREALIS)) {
    const guest_os::GuestOsRegistryService::Registration& registration =
        pair.second;
    apps.push_back(CreateApp(registration, /*generate_new_icon_key=*/true));
  }
  AppPublisher::Publish(std::move(apps), AppType::kBorealis,
                        /*should_notify_initialized=*/true);

  CallWithBorealisAllowed(base::BindOnce(&BorealisApps::SetUpSpecialApps,
                                         weak_factory_.GetWeakPtr()));
}

void BorealisApps::LoadIcon(const std::string& app_id,
                            const IconKey& icon_key,
                            IconType icon_type,
                            int32_t size_hint_in_dip,
                            bool allow_placeholder_icon,
                            apps::LoadIconCallback callback) {
  Registry()->LoadIcon(app_id, icon_key, icon_type, size_hint_in_dip,
                       allow_placeholder_icon, IconKey::kInvalidResourceId,
                       std::move(callback));
}

void BorealisApps::GetCompressedIconData(const std::string& app_id,
                                         int32_t size_in_dip,
                                         ui::ResourceScaleFactor scale_factor,
                                         LoadIconCallback callback) {
  GetGuestOSAppCompressedIconData(profile_, app_id, size_in_dip, scale_factor,
                                  std::move(callback));
}

void BorealisApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          LaunchSource launch_source,
                          WindowInfoPtr window_info) {
  borealis::BorealisService::GetForProfile(profile_)->AppLauncher().Launch(
      app_id, base::DoNothing());
}

void BorealisApps::LaunchAppWithParams(AppLaunchParams&& params,
                                       LaunchCallback callback) {
  Launch(params.app_id, ui::EF_NONE, LaunchSource::kUnknown, nullptr);

  // TODO(crbug.com/1244506): Add launch return value.
  std::move(callback).Run(LaunchResult());
}

void BorealisApps::SetPermission(const std::string& app_id,
                                 PermissionPtr permission_ptr) {
  auto permission = permission_ptr->permission_type;
  const char* pref_name = PermissionToPrefName(permission);
  if (!pref_name) {
    return;
  }
  profile_->GetPrefs()->SetBoolean(pref_name,
                                   permission_ptr->IsPermissionEnabled());
}

void BorealisApps::Uninstall(const std::string& app_id,
                             UninstallSource uninstall_source,
                             bool clear_site_data,
                             bool report_abuse) {
  borealis::BorealisService::GetForProfile(profile_)
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
  if (borealis::BorealisService::GetForProfile(profile_)
          ->Features()
          .IsEnabled() ||
      app_id == borealis::kClientAppId) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, menu_items);
  }

  // TODO(b/162562622): Menu models for borealis apps.

  std::move(callback).Run(std::move(menu_items));
}

void BorealisApps::OnRegistryUpdated(
    guest_os::GuestOsRegistryService* registry_service,
    guest_os::VmType vm_type,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  if (vm_type != guest_os::VmType::BOREALIS) {
    return;
  }

  for (const std::string& app_id : updated_apps) {
    if (auto registration = registry_service->GetRegistration(app_id)) {
      AppPublisher::Publish(
          CreateApp(*registration, /*generate_new_icon_key=*/false));
    }
  }

  for (const std::string& app_id : removed_apps) {
    auto app = std::make_unique<App>(AppType::kBorealis, app_id);
    app->readiness = Readiness::kUninstalledByUser;
    AppPublisher::Publish(std::move(app));
  }

  for (const std::string& app_id : inserted_apps) {
    if (auto registration = registry_service->GetRegistration(app_id)) {
      AppPublisher::Publish(
          CreateApp(*registration, /*generate_new_icon_key=*/true));
    }
  }
}

void BorealisApps::OnPermissionChanged() {
  auto app = std::make_unique<App>(AppType::kBorealis, borealis::kClientAppId);
  app->permissions = CreatePermissions(profile_);
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
  InitializeApp(*app, /*visible=*/false);

  app->icon_key = IconKey(IconKey::kDoesNotChangeOverTime,
                          IDR_LOGO_BOREALIS_DEFAULT_192, IconEffects::kNone);

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

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/borealis_apps.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
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
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/permission_utils.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

struct PermissionInfo {
  apps::mojom::PermissionType permission;
  const char* pref_name;
};

constexpr PermissionInfo permission_infos[] = {
    {apps::mojom::PermissionType::kMicrophone,
     borealis::prefs::kBorealisMicAllowed},
};

const char* PermissionToPrefName(apps::mojom::PermissionType permission) {
  for (const PermissionInfo& info : permission_infos) {
    if (info.permission == permission) {
      return info.pref_name;
    }
  }
  return nullptr;
}

void SetAppAllowed(apps::mojom::App* app,
                   bool allowed,
                   bool partially_hidden = false) {
  app->readiness = allowed ? apps::mojom::Readiness::kReady
                           : apps::mojom::Readiness::kDisabledByPolicy;

  const apps::mojom::OptionalBool opt_allowed =
      allowed ? apps::mojom::OptionalBool::kTrue
              : apps::mojom::OptionalBool::kFalse;
  const apps::mojom::OptionalBool opt_partial_allowed =
      allowed && !partially_hidden ? apps::mojom::OptionalBool::kTrue
                                   : apps::mojom::OptionalBool::kFalse;

  app->recommendable = opt_allowed;
  app->searchable = opt_allowed;
  app->show_in_launcher = opt_partial_allowed;
  app->show_in_shelf = opt_allowed;
  app->show_in_search = opt_partial_allowed;
  app->show_in_management = opt_partial_allowed;
  app->handles_intents = opt_allowed;
}

void SetAppAllowed(bool allowed,
                   apps::App& app,
                   bool partially_hidden = false) {
  app.readiness =
      allowed ? apps::Readiness::kReady : apps::Readiness::kDisabledByPolicy;

  app.recommendable = allowed;
  app.searchable = allowed;
  app.show_in_launcher = allowed && !partially_hidden;
  app.show_in_shelf = allowed;
  app.show_in_search = allowed && !partially_hidden;
  app.show_in_management = allowed && !partially_hidden;
  app.handles_intents = allowed;
}

apps::AppPtr CreateBorealisLauncher(Profile* profile, bool allowed) {
  auto app = apps::AppPublisher::MakeApp(
      apps::AppType::kBorealis, borealis::kInstallerAppId,
      allowed ? apps::Readiness::kReady : apps::Readiness::kDisabledByPolicy,
      l10n_util::GetStringUTF8(IDS_BOREALIS_APP_NAME),
      apps::InstallReason::kDefault, apps::InstallSource::kUnknown);

  app->icon_key =
      apps::IconKey(apps::IconKey::kDoesNotChangeOverTime,
                    IDR_LOGO_BOREALIS_DEFAULT_192, apps::IconEffects::kNone);

  SetAppAllowed(allowed, *app, /*partially_hidden=*/true);

  app->allow_uninstall =
      borealis::BorealisService::GetForProfile(profile)->Features().IsEnabled();

  // TODO(crbug.com/1253250): Add other fields for the App struct.
  return app;
}

apps::mojom::AppPtr GetBorealisLauncher(Profile* profile, bool allowed) {
  apps::mojom::AppPtr app = apps::PublisherBase::MakeApp(
      apps::mojom::AppType::kBorealis, borealis::kInstallerAppId,
      allowed ? apps::mojom::Readiness::kReady
              : apps::mojom::Readiness::kDisabledByPolicy,
      l10n_util::GetStringUTF8(IDS_BOREALIS_APP_NAME),
      apps::mojom::InstallReason::kDefault);

  app->icon_key = apps::mojom::IconKey::New(
      apps::mojom::IconKey::kDoesNotChangeOverTime,
      IDR_LOGO_BOREALIS_DEFAULT_192, apps::IconEffects::kNone);

  SetAppAllowed(app.get(), allowed, /*partially_hidden=*/true);

  app->allow_uninstall = (borealis::BorealisService::GetForProfile(profile)
                              ->Features()
                              .IsEnabled())
                             ? apps::mojom::OptionalBool::kTrue
                             : apps::mojom::OptionalBool::kFalse;

  return app;
}

// TODO(crbug.com/1253250): Remove and use CreatePermissions.
void PopulatePermissions(apps::mojom::App* app, Profile* profile) {
  for (const PermissionInfo& info : permission_infos) {
    auto permission = apps::mojom::Permission::New();
    permission->permission_type = info.permission;
    permission->value = apps::mojom::PermissionValue::NewBoolValue(
        profile->GetPrefs()->GetBoolean(info.pref_name));
    permission->is_managed = false;
    app->permissions.push_back(std::move(permission));
  }
}

apps::Permissions CreatePermissions(Profile* profile) {
  apps::Permissions permissions;
  for (const PermissionInfo& info : permission_infos) {
    permissions.push_back(std::make_unique<apps::Permission>(
        apps::ConvertMojomPermissionTypeToPermissionType(info.permission),
        std::make_unique<apps::PermissionValue>(
            profile->GetPrefs()->GetBoolean(info.pref_name)),
        /*is_managed=*/false));
  }
  return permissions;
}

bool IsBorealisLauncherAllowed(Profile* profile) {
  // TODO(b/217653546): The installer "app" was a short-term solution anyway.
  // Once insert_coin is a thing it should never be shown.
  return borealis::BorealisService::GetForProfile(profile)
                 ->Features()
                 .MightBeAllowed() ==
             borealis::BorealisFeatures::AllowStatus::kAllowed &&
         !borealis::BorealisService::GetForProfile(profile)
              ->Features()
              .IsEnabled();
}

}  // namespace

namespace apps {

BorealisApps::BorealisApps(AppServiceProxy* proxy)
    : AppPublisher(proxy), profile_(proxy->profile()) {
  Registry()->AddObserver(this);

  anonymous_app_observation_.Observe(
      &borealis::BorealisService::GetForProfile(profile_)->WindowManager());

  // TODO(b/170264723): When uninstalling borealis is completed, ensure that we
  // remove the apps from the apps service.
}

BorealisApps::~BorealisApps() {
  Registry()->RemoveObserver(this);
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
  DCHECK_EQ(registration.VmType(), guest_os::GuestOsRegistryService::VmType::
                                       ApplicationList_VmType_BOREALIS);

  // The installer app is not a GuestOs app, it doesnt have a registration and
  // it can't be converted.
  DCHECK_NE(registration.app_id(), borealis::kInstallerAppId);

  auto app = AppPublisher::MakeApp(
      AppType::kBorealis, registration.app_id(), Readiness::kReady,
      registration.Name(), InstallReason::kUser, InstallSource::kUnknown);

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

  SetAppAllowed(!registration.NoDisplay(), *app);

  // TODO(crbug.com/1253250): Add other fields for the App struct.
  return app;
}

apps::mojom::AppPtr BorealisApps::Convert(
    const guest_os::GuestOsRegistryService::Registration& registration,
    bool new_icon_key) {
  // We must only convert borealis apps.
  DCHECK_EQ(registration.VmType(), guest_os::GuestOsRegistryService::VmType::
                                       ApplicationList_VmType_BOREALIS);

  // The installer app is not a GuestOs app, it doesnt have a registration and
  // it can't be converted.
  DCHECK_NE(registration.app_id(), borealis::kInstallerAppId);

  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      apps::mojom::AppType::kBorealis, registration.app_id(),
      apps::mojom::Readiness::kReady, registration.Name(),
      apps::mojom::InstallReason::kUser);

  const std::string& executable_file_name = registration.ExecutableFileName();
  if (!executable_file_name.empty()) {
    app->additional_search_terms.push_back(executable_file_name);
  }

  for (const std::string& keyword : registration.Keywords()) {
    app->additional_search_terms.push_back(keyword);
  }

  if (new_icon_key) {
    auto icon_effects = IconEffects::kCrOsStandardIcon;
    app->icon_key = icon_key_factory_.MakeIconKey(icon_effects);
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  if (registration.app_id() == borealis::kClientAppId) {
    PopulatePermissions(app.get(), profile_);
  }

  SetAppAllowed(app.get(), !registration.NoDisplay());
  return app;
}

void BorealisApps::Initialize() {
  PublisherBase::Initialize(proxy()->AppService(),
                            apps::mojom::AppType::kBorealis);

  RegisterPublisher(AppType::kBorealis);

  std::vector<AppPtr> apps;
  apps.push_back(
      CreateBorealisLauncher(profile_, IsBorealisLauncherAllowed(profile_)));

  for (const auto& pair :
       Registry()->GetRegisteredApps(guest_os::GuestOsRegistryService::VmType::
                                         ApplicationList_VmType_BOREALIS)) {
    const guest_os::GuestOsRegistryService::Registration& registration =
        pair.second;
    apps.push_back(CreateApp(registration, /*generate_new_icon_key=*/true));
  }
  AppPublisher::Publish(std::move(apps), AppType::kBorealis,
                        /*should_notify_initialized=*/true);
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

void BorealisApps::LaunchAppWithParams(AppLaunchParams&& params,
                                       LaunchCallback callback) {
  Launch(params.app_id, ui::EF_NONE, apps::mojom::LaunchSource::kUnknown,
         nullptr);
  // TODO(crbug.com/1244506): Add launch return value.
  std::move(callback).Run(LaunchResult());
}

void BorealisApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(
      GetBorealisLauncher(profile_, IsBorealisLauncherAllowed(profile_)));

  for (const auto& pair :
       Registry()->GetRegisteredApps(guest_os::GuestOsRegistryService::VmType::
                                         ApplicationList_VmType_BOREALIS)) {
    const guest_os::GuestOsRegistryService::Registration& registration =
        pair.second;
    apps.push_back(Convert(registration, /*new_icon_key=*/true));
  }

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), apps::mojom::AppType::kBorealis,
                     true /* should_notify_initialized */);
  subscribers_.Add(std::move(subscriber));
}

void BorealisApps::LoadIcon(const std::string& app_id,
                            apps::mojom::IconKeyPtr icon_key,
                            apps::mojom::IconType icon_type,
                            int32_t size_hint_in_dip,
                            bool allow_placeholder_icon,
                            LoadIconCallback callback) {
  if (!icon_key) {
    // On failure, we still run the callback, with an empty IconValue.
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  std::unique_ptr<IconKey> key = ConvertMojomIconKeyToIconKey(icon_key);
  Registry()->LoadIcon(app_id, *key, ConvertMojomIconTypeToIconType(icon_type),
                       size_hint_in_dip, allow_placeholder_icon,
                       apps::mojom::IconKey::kInvalidResourceId,
                       IconValueToMojomIconValueCallback(std::move(callback)));
}

void BorealisApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::WindowInfoPtr window_info) {
  borealis::BorealisService::GetForProfile(profile_)->AppLauncher().Launch(
      app_id, base::DoNothing());
}

void BorealisApps::SetPermission(const std::string& app_id,
                                 apps::mojom::PermissionPtr permission_ptr) {
  auto permission = permission_ptr->permission_type;
  const char* pref_name = PermissionToPrefName(permission);
  if (!pref_name) {
    return;
  }
  profile_->GetPrefs()->SetBoolean(
      pref_name, apps_util::IsPermissionEnabled(permission_ptr->value));
}

void BorealisApps::Uninstall(const std::string& app_id,
                             apps::mojom::UninstallSource uninstall_source,
                             bool clear_site_data,
                             bool report_abuse) {
  borealis::BorealisService::GetForProfile(profile_)
      ->AppUninstaller()
      .Uninstall(app_id, base::DoNothing());
}

void BorealisApps::GetMenuModel(const std::string& app_id,
                                apps::mojom::MenuType menu_type,
                                int64_t display_id,
                                GetMenuModelCallback callback) {
  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  if (borealis::BorealisService::GetForProfile(profile_)
          ->Features()
          .IsEnabled()) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, &menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, &menu_items);
  }

  // TODO(b/162562622): Menu models for borealis apps.

  std::move(callback).Run(std::move(menu_items));
}

void BorealisApps::OnRegistryUpdated(
    guest_os::GuestOsRegistryService* registry_service,
    guest_os::GuestOsRegistryService::VmType vm_type,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  if (vm_type != guest_os::GuestOsRegistryService::VmType::
                     ApplicationList_VmType_BOREALIS) {
    return;
  }

  for (const std::string& app_id : updated_apps) {
    if (auto registration = registry_service->GetRegistration(app_id)) {
      PublisherBase::Publish(Convert(*registration, /*new_icon_key=*/false),
                             subscribers_);
      AppPublisher::Publish(
          CreateApp(*registration, /*generate_new_icon_key=*/false));
    }
  }

  for (const std::string& app_id : removed_apps) {
    apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
    mojom_app->app_type = apps::mojom::AppType::kBorealis;
    mojom_app->app_id = app_id;
    mojom_app->readiness = apps::mojom::Readiness::kUninstalledByUser;
    PublisherBase::Publish(std::move(mojom_app), subscribers_);

    auto app = std::make_unique<App>(AppType::kBorealis, app_id);
    app->readiness = Readiness::kUninstalledByUser;
    AppPublisher::Publish(std::move(app));
  }

  for (const std::string& app_id : inserted_apps) {
    if (auto registration = registry_service->GetRegistration(app_id)) {
      PublisherBase::Publish(Convert(*registration, /*new_icon_key=*/true),
                             subscribers_);
      AppPublisher::Publish(
          CreateApp(*registration, /*generate_new_icon_key=*/true));
    }
  }
}

void BorealisApps::OnAnonymousAppAdded(const std::string& shelf_app_id,
                                       const std::string& shelf_app_name) {
  apps::mojom::AppPtr mojom_app = apps::PublisherBase::MakeApp(
      apps::mojom::AppType::kBorealis, shelf_app_id,
      apps::mojom::Readiness::kReady, shelf_app_name,
      apps::mojom::InstallReason::kUser);

  mojom_app->icon_key = apps::mojom::IconKey::New(
      apps::mojom::IconKey::kDoesNotChangeOverTime,
      IDR_LOGO_BOREALIS_DEFAULT_192, apps::IconEffects::kNone);
  mojom_app->recommendable = apps::mojom::OptionalBool::kFalse;
  mojom_app->searchable = apps::mojom::OptionalBool::kFalse;
  mojom_app->show_in_launcher = apps::mojom::OptionalBool::kFalse;
  mojom_app->show_in_shelf = apps::mojom::OptionalBool::kTrue;
  mojom_app->show_in_search = apps::mojom::OptionalBool::kFalse;

  PublisherBase::Publish(std::move(mojom_app), subscribers_);

  auto app = AppPublisher::MakeApp(
      AppType::kBorealis, shelf_app_id, Readiness::kReady, shelf_app_name,
      InstallReason::kUser, InstallSource::kUnknown);

  app->icon_key = IconKey(IconKey::kDoesNotChangeOverTime,
                          IDR_LOGO_BOREALIS_DEFAULT_192, IconEffects::kNone);
  app->recommendable = false;
  app->searchable = false;
  app->show_in_launcher = false;
  app->show_in_shelf = true;
  app->show_in_search = false;

  AppPublisher::Publish(std::move(app));
}

void BorealisApps::OnAnonymousAppRemoved(const std::string& shelf_app_id) {
  // First uninstall the anonymous app, then remove it.
  for (auto readiness : {apps::mojom::Readiness::kUninstalledByUser,
                         apps::mojom::Readiness::kRemoved}) {
    apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
    mojom_app->app_type = apps::mojom::AppType::kBorealis;
    mojom_app->app_id = shelf_app_id;
    mojom_app->readiness = readiness;
    PublisherBase::Publish(std::move(mojom_app), subscribers_);
  }

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

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/plugin_vm_apps.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/permission_utils.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

struct PermissionInfo {
  apps::PermissionType permission;
  const char* pref_name;
};

// TODO(crbug.com/1198390): Update to use a switch to map between two enum.
constexpr PermissionInfo permission_infos[] = {
    {apps::PermissionType::kPrinting,
     plugin_vm::prefs::kPluginVmPrintersAllowed},
    {apps::PermissionType::kCamera, plugin_vm::prefs::kPluginVmCameraAllowed},
    {apps::PermissionType::kMicrophone, plugin_vm::prefs::kPluginVmMicAllowed},
};

const char* PermissionToPrefName(apps::PermissionType permission) {
  for (const PermissionInfo& info : permission_infos) {
    if (info.permission == permission) {
      return info.pref_name;
    }
  }
  return nullptr;
}

void SetAppAllowed(apps::mojom::App* app, bool allowed) {
  app->readiness = allowed ? apps::mojom::Readiness::kReady
                           : apps::mojom::Readiness::kDisabledByPolicy;

  const apps::mojom::OptionalBool opt_allowed =
      allowed ? apps::mojom::OptionalBool::kTrue
              : apps::mojom::OptionalBool::kFalse;

  app->recommendable = opt_allowed;
  app->searchable = opt_allowed;
  app->show_in_launcher = opt_allowed;
  app->show_in_shelf = opt_allowed;
  app->show_in_search = opt_allowed;
  app->handles_intents = opt_allowed;
}

void SetAppAllowed(bool allowed, apps::App& app) {
  app.readiness =
      allowed ? apps::Readiness::kReady : apps::Readiness::kDisabledByPolicy;
  app.recommendable = allowed;
  app.searchable = allowed;
  app.show_in_launcher = allowed;
  app.show_in_shelf = allowed;
  app.show_in_search = allowed;
  app.handles_intents = allowed;
}

void SetShowInAppManagement(apps::mojom::App* app, bool installed) {
  // Show when installed, even if disabled by policy, to give users the choice
  // to uninstall and free up space.
  app->show_in_management = installed ? apps::mojom::OptionalBool::kTrue
                                      : apps::mojom::OptionalBool::kFalse;
}

// TODO(crbug.com/1253250): Remove and use CreatePermissions.
void PopulatePermissions(apps::mojom::App* app, Profile* profile) {
  for (const PermissionInfo& info : permission_infos) {
    auto permission = apps::mojom::Permission::New();
    permission->permission_type =
        apps::ConvertPermissionTypeToMojomPermissionType(info.permission);
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
        info.permission,
        std::make_unique<apps::PermissionValue>(
            profile->GetPrefs()->GetBoolean(info.pref_name)),
        /*is_managed=*/false));
  }
  return permissions;
}

apps::AppPtr CreatePluginVmApp(Profile* profile, bool allowed) {
  auto app = apps::AppPublisher::MakeApp(
      apps::AppType::kPluginVm, plugin_vm::kPluginVmShelfAppId,
      allowed ? apps::Readiness::kReady : apps::Readiness::kDisabledByPolicy,
      l10n_util::GetStringUTF8(IDS_PLUGIN_VM_APP_NAME),
      apps::InstallReason::kUser, apps::InstallSource::kUnknown);

  app->icon_key =
      apps::IconKey(apps::IconKey::kDoesNotChangeOverTime,
                    IDR_LOGO_PLUGIN_VM_DEFAULT_192, apps::IconEffects::kNone);

  app->permissions = CreatePermissions(profile);

  SetAppAllowed(allowed, *app);

  // Show when installed, even if disabled by policy, to give users the choice
  // to uninstall and free up space.
  app->show_in_management =
      plugin_vm::PluginVmFeatures::Get()->IsConfigured(profile);

  // TODO(crbug.com/1253250): Add other fields for the App struct.
  return app;
}

apps::mojom::AppPtr GetPluginVmApp(Profile* profile, bool allowed) {
  apps::mojom::AppPtr app = apps::PublisherBase::MakeApp(
      apps::mojom::AppType::kPluginVm, plugin_vm::kPluginVmShelfAppId,
      allowed ? apps::mojom::Readiness::kReady
              : apps::mojom::Readiness::kDisabledByPolicy,
      l10n_util::GetStringUTF8(IDS_PLUGIN_VM_APP_NAME),
      apps::mojom::InstallReason::kUser);

  app->icon_key = apps::mojom::IconKey::New(
      apps::mojom::IconKey::kDoesNotChangeOverTime,
      IDR_LOGO_PLUGIN_VM_DEFAULT_192, apps::IconEffects::kNone);

  SetShowInAppManagement(
      app.get(), plugin_vm::PluginVmFeatures::Get()->IsConfigured(profile));
  PopulatePermissions(app.get(), profile);
  SetAppAllowed(app.get(), allowed);

  return app;
}

// Create a file intent filter with extension type conditions for App Service.
apps::IntentFilters CreateIntentFilterForPluginVm(
    const guest_os::GuestOsRegistryService::Registration& registration) {
  const std::set<std::string>& extension_types_set = registration.Extensions();
  if (extension_types_set.empty()) {
    return {};
  }
  std::vector<std::string> extension_types(extension_types_set.begin(),
                                           extension_types_set.end());
  apps::IntentFilters intent_filters;
  intent_filters.push_back(apps_util::CreateFileFilter(
      {apps_util::kIntentActionView}, /*mime_types=*/{}, extension_types,
      // TODO(crbug/1349974): Remove activity_name when default file handling
      // preferences for Files App are migrated.
      /*activity_name=*/apps_util::kGuestOsActivityName));

  return intent_filters;
}

apps::LaunchResult ConvertPluginVmResultToLaunchResult(
    plugin_vm::LaunchPluginVmAppResult plugin_vm_result) {
  switch (plugin_vm_result) {
    case plugin_vm::LaunchPluginVmAppResult::SUCCESS:
      return apps::LaunchResult(apps::State::SUCCESS);
    case plugin_vm::LaunchPluginVmAppResult::FAILED_DIRECTORY_NOT_SHARED:
      return apps::LaunchResult(apps::State::FAILED_DIRECTORY_NOT_SHARED);
    case plugin_vm::LaunchPluginVmAppResult::FAILED:
      return apps::LaunchResult(apps::State::FAILED);
  }
}

}  // namespace

namespace apps {

PluginVmApps::PluginVmApps(AppServiceProxy* proxy)
    : AppPublisher(proxy), profile_(proxy->profile()) {
  // Don't show anything for non-primary profiles. We can't use
  // `PluginVmFeatures::Get()->IsAllowed()` here because we still let the user
  // uninstall Plugin VM when it isn't allowed for some other reasons (e.g.
  // policy).
  if (!ash::ProfileHelper::IsPrimaryProfile(profile_)) {
    return;
  }

  // Register for Plugin VM changes to policy and installed state, so that we
  // can update the availability and status of the Plugin VM app. Unretained is
  // safe as these are cleaned up upon destruction.
  policy_subscription_ =
      std::make_unique<plugin_vm::PluginVmPolicySubscription>(
          profile_, base::BindRepeating(&PluginVmApps::OnPluginVmAllowedChanged,
                                        base::Unretained(this)));
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      plugin_vm::prefs::kPluginVmImageExists,
      base::BindRepeating(&PluginVmApps::OnPluginVmConfiguredChanged,
                          base::Unretained(this)));
  for (const PermissionInfo& info : permission_infos) {
    pref_registrar_.Add(info.pref_name,
                        base::BindRepeating(&PluginVmApps::OnPermissionChanged,
                                            base::Unretained(this)));
  }

  is_allowed_ = plugin_vm::PluginVmFeatures::Get()->IsAllowed(profile_);
}

PluginVmApps::~PluginVmApps() {
  if (registry_) {
    registry_->RemoveObserver(this);
  }
}

void PluginVmApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(GetPluginVmApp(profile_, is_allowed_));

  for (const auto& pair :
       registry_->GetRegisteredApps(guest_os::VmType::PLUGIN_VM)) {
    const guest_os::GuestOsRegistryService::Registration& registration =
        pair.second;
    apps.push_back(Convert(registration, /*new_icon_key=*/true));
  }

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), apps::mojom::AppType::kPluginVm,
                     true /* should_notify_initialized */);
  subscribers_.Add(std::move(subscriber));
}

void PluginVmApps::Initialize() {
  registry_ = guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_);
  if (!registry_) {
    return;
  }
  registry_->AddObserver(this);

  PublisherBase::Initialize(proxy()->AppService(),
                            apps::mojom::AppType::kPluginVm);

  RegisterPublisher(AppType::kPluginVm);

  std::vector<AppPtr> apps;
  apps.push_back(CreatePluginVmApp(profile_, is_allowed_));
  for (const auto& pair :
       registry_->GetRegisteredApps(guest_os::VmType::PLUGIN_VM)) {
    const guest_os::GuestOsRegistryService::Registration& registration =
        pair.second;
    apps.push_back(CreateApp(registration, /*generate_new_icon_key=*/true));
  }
  AppPublisher::Publish(std::move(apps), AppType::kPluginVm,
                        /*should_notify_initialized=*/true);
}

void PluginVmApps::LoadIcon(const std::string& app_id,
                            const IconKey& icon_key,
                            IconType icon_type,
                            int32_t size_hint_in_dip,
                            bool allow_placeholder_icon,
                            apps::LoadIconCallback callback) {
  registry_->LoadIcon(app_id, icon_key, icon_type, size_hint_in_dip,
                      allow_placeholder_icon, IconKey::kInvalidResourceId,
                      std::move(callback));
}

void PluginVmApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          LaunchSource launch_source,
                          WindowInfoPtr window_info) {
  DCHECK_EQ(plugin_vm::kPluginVmShelfAppId, app_id);
  if (plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile_)) {
    plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)->LaunchPluginVm(
        base::DoNothing());
  } else {
    plugin_vm::ShowPluginVmInstallerView(profile_);
  }
}

void PluginVmApps::LaunchAppWithIntent(const std::string& app_id,
                                       int32_t event_flags,
                                       IntentPtr intent,
                                       LaunchSource launch_source,
                                       WindowInfoPtr window_info,
                                       LaunchCallback callback) {
  // Retrieve URLs from the files in the intent.
  std::vector<plugin_vm::LaunchArg> args;
  if (intent && intent->files.size() > 0) {
    args.reserve(intent->files.size());
    storage::FileSystemContext* file_system_context =
        file_manager::util::GetFileManagerFileSystemContext(profile_);
    for (auto& file : intent->files) {
      args.emplace_back(
          file_system_context->CrackURLInFirstPartyContext(file->url));
    }
  }
  plugin_vm::LaunchPluginVmApp(
      profile_, app_id, args,
      base::BindOnce(
          [](LaunchCallback callback,
             plugin_vm::LaunchPluginVmAppResult plugin_vm_result,
             const std::string& failure_reason) {
            if (plugin_vm_result !=
                plugin_vm::LaunchPluginVmAppResult::SUCCESS) {
              LOG(ERROR) << "Plugin VM launch error: " << failure_reason;
            }
            std::move(callback).Run(
                ConvertPluginVmResultToLaunchResult(plugin_vm_result));
          },
          std::move(callback)));
}

void PluginVmApps::LaunchAppWithParams(AppLaunchParams&& params,
                                       LaunchCallback callback) {
  Launch(params.app_id, ui::EF_NONE, LaunchSource::kUnknown, nullptr);

  // TODO(crbug.com/1244506): Add launch return value.
  std::move(callback).Run(LaunchResult());
}

void PluginVmApps::SetPermission(const std::string& app_id,
                                 PermissionPtr permission_ptr) {
  auto permission = permission_ptr->permission_type;
  const char* pref_name = PermissionToPrefName(permission);
  if (!pref_name) {
    return;
  }

  profile_->GetPrefs()->SetBoolean(pref_name,
                                   permission_ptr->IsPermissionEnabled());
}

void PluginVmApps::Uninstall(const std::string& app_id,
                             UninstallSource uninstall_source,
                             bool clear_site_data,
                             bool report_abuse) {
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
      ->ClearApplicationList(guest_os::VmType::PLUGIN_VM,
                             plugin_vm::kPluginVmName, "");
  plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)
      ->UninstallPluginVm();
}

void PluginVmApps::GetMenuModel(const std::string& app_id,
                                MenuType menu_type,
                                int64_t display_id,
                                base::OnceCallback<void(MenuItems)> callback) {
  MenuItems menu_items;

  if (ShouldAddOpenItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::LAUNCH_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                   menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, menu_items);
  }

  if (app_id == plugin_vm::kPluginVmShelfAppId &&
      plugin_vm::IsPluginVmRunning(profile_)) {
    AddCommandItem(ash::SHUTDOWN_GUEST_OS, IDS_PLUGIN_VM_SHUT_DOWN_MENU_ITEM,
                   menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

void PluginVmApps::OnRegistryUpdated(
    guest_os::GuestOsRegistryService* registry_service,
    guest_os::VmType vm_type,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  if (vm_type != guest_os::VmType::PLUGIN_VM) {
    return;
  }

  for (const std::string& app_id : updated_apps) {
    if (auto registration = registry_->GetRegistration(app_id)) {
      PublisherBase::Publish(Convert(*registration, /*new_icon_key=*/false),
                             subscribers_);
      AppPublisher::Publish(
          CreateApp(*registration, /*generate_new_icon_key=*/false));
    }
  }
  for (const std::string& app_id : removed_apps) {
    apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
    mojom_app->app_type = apps::mojom::AppType::kPluginVm;
    mojom_app->app_id = app_id;
    mojom_app->readiness = apps::mojom::Readiness::kUninstalledByUser;
    PublisherBase::Publish(std::move(mojom_app), subscribers_);

    auto app = std::make_unique<App>(AppType::kPluginVm, app_id);
    app->readiness = apps::Readiness::kUninstalledByUser;
    AppPublisher::Publish(std::move(app));
  }
  for (const std::string& app_id : inserted_apps) {
    if (auto registration = registry_->GetRegistration(app_id)) {
      PublisherBase::Publish(Convert(*registration, /*new_icon_key=*/true),
                             subscribers_);
      AppPublisher::Publish(
          CreateApp(*registration, /*generate_new_icon_key=*/true));
    }
  }
}

AppPtr PluginVmApps::CreateApp(
    const guest_os::GuestOsRegistryService::Registration& registration,
    bool generate_new_icon_key) {
  DCHECK_EQ(registration.VmType(), guest_os::VmType::PLUGIN_VM);

  auto app = AppPublisher::MakeApp(
      AppType::kPluginVm, registration.app_id(), Readiness::kReady,
      registration.Name(), InstallReason::kUser, apps::InstallSource::kUnknown);

  if (generate_new_icon_key) {
    app->icon_key = std::move(
        *icon_key_factory_.CreateIconKey(IconEffects::kCrOsStandardIcon));
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  app->show_in_launcher = false;
  app->show_in_search = false;
  app->show_in_shelf = false;
  app->show_in_management = false;
  app->allow_uninstall = false;
  app->handles_intents = true;
  app->intent_filters = CreateIntentFilterForPluginVm(registration);

  // TODO(crbug.com/1253250): Add other fields for the App struct.
  return app;
}

apps::mojom::AppPtr PluginVmApps::Convert(
    const guest_os::GuestOsRegistryService::Registration& registration,
    bool new_icon_key) {
  DCHECK_EQ(registration.VmType(), guest_os::VmType::PLUGIN_VM);

  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      apps::mojom::AppType::kPluginVm, registration.app_id(),
      apps::mojom::Readiness::kReady, registration.Name(),
      apps::mojom::InstallReason::kUser);

  if (new_icon_key) {
    auto icon_effects = IconEffects::kCrOsStandardIcon;
    app->icon_key = icon_key_factory_.MakeIconKey(icon_effects);
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  app->show_in_launcher = apps::mojom::OptionalBool::kFalse;
  app->show_in_search = apps::mojom::OptionalBool::kFalse;
  app->show_in_shelf = apps::mojom::OptionalBool::kFalse;
  app->show_in_management = apps::mojom::OptionalBool::kFalse;
  app->allow_uninstall = apps::mojom::OptionalBool::kFalse;
  app->handles_intents = apps::mojom::OptionalBool::kTrue;
  app->intent_filters = ConvertIntentFiltersToMojomIntentFilters(
      CreateIntentFilterForPluginVm(registration));

  return app;
}

void PluginVmApps::OnPluginVmAllowedChanged(bool is_allowed) {
  // Republish the Plugin VM app when policy changes have changed
  // its availability. Only changed fields need to be republished.
  is_allowed_ = is_allowed;

  apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
  mojom_app->app_type = apps::mojom::AppType::kPluginVm;
  mojom_app->app_id = plugin_vm::kPluginVmShelfAppId;
  SetAppAllowed(mojom_app.get(), is_allowed);
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

  auto app =
      std::make_unique<App>(AppType::kPluginVm, plugin_vm::kPluginVmShelfAppId);
  SetAppAllowed(is_allowed, *app);
  AppPublisher::Publish(std::move(app));
}

void PluginVmApps::OnPluginVmConfiguredChanged() {
  // Only changed fields need to be republished.
  apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
  mojom_app->app_type = apps::mojom::AppType::kPluginVm;
  mojom_app->app_id = plugin_vm::kPluginVmShelfAppId;
  SetShowInAppManagement(
      mojom_app.get(),
      plugin_vm::PluginVmFeatures::Get()->IsConfigured(profile_));
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

  auto app =
      std::make_unique<App>(AppType::kPluginVm, plugin_vm::kPluginVmShelfAppId);
  app->show_in_management =
      plugin_vm::PluginVmFeatures::Get()->IsConfigured(profile_);
  AppPublisher::Publish(std::move(app));
}

void PluginVmApps::OnPermissionChanged() {
  apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
  mojom_app->app_type = apps::mojom::AppType::kPluginVm;
  mojom_app->app_id = plugin_vm::kPluginVmShelfAppId;
  PopulatePermissions(mojom_app.get(), profile_);
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

  auto app =
      std::make_unique<App>(AppType::kPluginVm, plugin_vm::kPluginVmShelfAppId);
  app->permissions = CreatePermissions(profile_);
  AppPublisher::Publish(std::move(app));
}

}  // namespace apps

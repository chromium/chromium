// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/borealis_apps.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_app_uninstaller.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

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
}

apps::mojom::AppPtr GetBorealisLauncher(Profile* profile, bool allowed) {
  apps::mojom::AppPtr app = apps::PublisherBase::MakeApp(
      apps::mojom::AppType::kBorealis, borealis::kBorealisAppId,
      allowed ? apps::mojom::Readiness::kReady
              : apps::mojom::Readiness::kDisabledByPolicy,
      l10n_util::GetStringUTF8(IDS_BOREALIS_APP_NAME),
      apps::mojom::InstallSource::kUser);

  app->icon_key = apps::mojom::IconKey::New(
      apps::mojom::IconKey::kDoesNotChangeOverTime,
      IDR_LOGO_BOREALIS_DEFAULT_192, apps::IconEffects::kNone);

  SetAppAllowed(app.get(), allowed);
  return app;
}

}  // namespace

namespace apps {

BorealisApps::BorealisApps(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile)
    : profile_(profile) {
  Registry()->AddObserver(this);

  anonymous_app_observation_.Observe(
      &borealis::BorealisService::GetForProfile(profile_)->WindowManager());

  PublisherBase::Initialize(app_service, apps::mojom::AppType::kBorealis);

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

apps::mojom::AppPtr BorealisApps::Convert(
    const guest_os::GuestOsRegistryService::Registration& registration,
    bool new_icon_key) {
  // We must only convert borealis apps.
  DCHECK_EQ(registration.VmType(), guest_os::GuestOsRegistryService::VmType::
                                       ApplicationList_VmType_BOREALIS);

  // The installer app is not a GuestOs app, it doesnt have a registration and
  // it can't be converted.
  DCHECK_NE(registration.app_id(), borealis::kBorealisAppId);

  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      apps::mojom::AppType::kBorealis, registration.app_id(),
      apps::mojom::Readiness::kReady, registration.Name(),
      apps::mojom::InstallSource::kUser);

  const std::string& executable_file_name = registration.ExecutableFileName();
  if (!executable_file_name.empty()) {
    app->additional_search_terms.push_back(executable_file_name);
  }

  for (const std::string& keyword : registration.Keywords()) {
    app->additional_search_terms.push_back(keyword);
  }

  if (new_icon_key) {
    auto icon_effects =
        base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)
            ? IconEffects::kCrOsStandardIcon
            : IconEffects::kNone;
    app->icon_key = icon_key_factory_.MakeIconKey(icon_effects);
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  SetAppAllowed(app.get(), !registration.NoDisplay());
  return app;
}

void BorealisApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(GetBorealisLauncher(
      profile_, borealis::BorealisService::GetForProfile(profile_)
                    ->Features()
                    .IsAllowed()));

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
  Registry()->LoadIcon(app_id, std::move(icon_key), icon_type, size_hint_in_dip,
                       allow_placeholder_icon,
                       apps::mojom::IconKey::kInvalidResourceId,
                       std::move(callback));
}

void BorealisApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::WindowInfoPtr window_info) {
  borealis::BorealisService::GetForProfile(profile_)->AppLauncher().Launch(
      app_id, base::DoNothing());
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

  // TODO(b/171353248): Uninstall for individual apps (not just the parent one).
  if (app_id == borealis::kBorealisAppId &&
      borealis::BorealisService::GetForProfile(profile_)
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
      Publish(Convert(*registration, /*new_icon_key=*/false), subscribers_);
    }
  }

  for (const std::string& app_id : removed_apps) {
    apps::mojom::AppPtr app = apps::mojom::App::New();
    app->app_type = apps::mojom::AppType::kBorealis;
    app->app_id = app_id;
    app->readiness = apps::mojom::Readiness::kUninstalledByUser;
    Publish(std::move(app), subscribers_);
  }

  for (const std::string& app_id : inserted_apps) {
    if (auto registration = registry_service->GetRegistration(app_id)) {
      Publish(Convert(*registration, /*new_icon_key=*/true), subscribers_);
    }
  }
}

void BorealisApps::OnAnonymousAppAdded(const std::string& shelf_app_id,
                                       const std::string& shelf_app_name) {
  apps::mojom::AppPtr app = apps::PublisherBase::MakeApp(
      apps::mojom::AppType::kBorealis, shelf_app_id,
      apps::mojom::Readiness::kReady, shelf_app_name,
      apps::mojom::InstallSource::kUser);

  app->icon_key = apps::mojom::IconKey::New(
      apps::mojom::IconKey::kDoesNotChangeOverTime,
      IDR_LOGO_BOREALIS_DEFAULT_192, apps::IconEffects::kNone);
  app->recommendable = apps::mojom::OptionalBool::kFalse;
  app->searchable = apps::mojom::OptionalBool::kFalse;
  app->show_in_launcher = apps::mojom::OptionalBool::kFalse;
  app->show_in_shelf = apps::mojom::OptionalBool::kTrue;
  app->show_in_search = apps::mojom::OptionalBool::kFalse;

  Publish(std::move(app), subscribers_);
}

void BorealisApps::OnAnonymousAppRemoved(const std::string& shelf_app_id) {
  // First uninstall the anonymous app, then remove it.
  for (auto readiness : {apps::mojom::Readiness::kUninstalledByUser,
                         apps::mojom::Readiness::kRemoved}) {
    apps::mojom::AppPtr app = apps::mojom::App::New();
    app->app_type = apps::mojom::AppType::kBorealis;
    app->app_id = shelf_app_id;
    app->readiness = readiness;
    Publish(std::move(app), subscribers_);
  }
}

void BorealisApps::OnWindowManagerDeleted(
    borealis::BorealisWindowManager* window_manager) {
  anonymous_app_observation_.Reset();
}

}  // namespace apps

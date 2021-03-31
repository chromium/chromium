// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/crostini_apps.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_package_service.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_shelf_utils.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"

// TODO(crbug.com/826982): the equivalent of
// CrostiniAppModelBuilder::MaybeCreateRootFolder. Does some sort of "root
// folder" abstraction belong here (on the publisher side of the App Service)
// or should we hard-code that in one particular subscriber (the App List UI)?

namespace {

bool ShouldShowDisplayDensityMenuItem(const std::string& app_id,
                                      apps::mojom::MenuType menu_type,
                                      int64_t display_id) {
  // The default terminal app is crosh in a Chrome window and it doesn't run in
  // the Crostini container so it doesn't support display density the same way.
  if (menu_type != apps::mojom::MenuType::kShelf ||
      app_id == crostini::kCrostiniTerminalSystemAppId) {
    return false;
  }

  display::Display d;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id, &d)) {
    return true;
  }

  return d.device_scale_factor() != 1.0;
}

}  // namespace

namespace apps {

CrostiniApps::CrostiniApps(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile)
    : profile_(profile), registry_(nullptr), crostini_enabled_(false) {
  Initialize(app_service);
}

CrostiniApps::~CrostiniApps() {
  if (registry_) {
    registry_->RemoveObserver(this);
  }
}

void CrostiniApps::ReInitializeForTesting(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile) {
  // Some test code creates a profile (and therefore profile-linked services
  // like the App Service) before it creates the fake user that lets
  // IsCrostiniUIAllowedForProfile return true. To work around that, we issue a
  // second Initialize call.
  receiver().reset();
  profile_ = profile;
  registry_ = nullptr;
  crostini_enabled_ = false;

  Initialize(app_service);
}

void CrostiniApps::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  if (!crostini::CrostiniFeatures::Get()->CouldBeAllowed(profile_)) {
    return;
  }
  registry_ = guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_);
  if (!registry_) {
    return;
  }
  crostini_enabled_ = crostini::CrostiniFeatures::Get()->IsEnabled(profile_);

  registry_->AddObserver(this);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      crostini::prefs::kCrostiniEnabled,
      base::BindRepeating(&CrostiniApps::OnCrostiniEnabledChanged,
                          base::Unretained(this)));

  PublisherBase::Initialize(app_service, apps::mojom::AppType::kCrostini);
}

void CrostiniApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;

  for (const auto& pair :
       registry_->GetRegisteredApps(guest_os::GuestOsRegistryService::VmType::
                                        ApplicationList_VmType_TERMINA)) {
    const guest_os::GuestOsRegistryService::Registration& registration =
        pair.second;
    apps.push_back(Convert(registration, /*new_icon_key=*/true));
  }

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), apps::mojom::AppType::kCrostini,
                     true /* should_notify_initialized */);
  subscribers_.Add(std::move(subscriber));
}

void CrostiniApps::LoadIcon(const std::string& app_id,
                            apps::mojom::IconKeyPtr icon_key,
                            apps::mojom::IconType icon_type,
                            int32_t size_hint_in_dip,
                            bool allow_placeholder_icon,
                            LoadIconCallback callback) {
  registry_->LoadIcon(app_id, std::move(icon_key), icon_type, size_hint_in_dip,
                      allow_placeholder_icon, IDR_LOGO_CROSTINI_DEFAULT,
                      std::move(callback));
}

void CrostiniApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::WindowInfoPtr window_info) {
  crostini::LaunchCrostiniApp(
      profile_, app_id,
      window_info ? window_info->display_id : display::kInvalidDisplayId);
}

void CrostiniApps::Uninstall(const std::string& app_id,
                             apps::mojom::UninstallSource uninstall_source,
                             bool clear_site_data,
                             bool report_abuse) {
  crostini::CrostiniPackageService::GetForProfile(profile_)
      ->QueueUninstallApplication(app_id);
}

void CrostiniApps::GetMenuModel(const std::string& app_id,
                                apps::mojom::MenuType menu_type,
                                int64_t display_id,
                                GetMenuModelCallback callback) {
  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  if (menu_type == apps::mojom::MenuType::kShelf) {
    AddCommandItem(ash::MENU_NEW_WINDOW, IDS_APP_LIST_NEW_WINDOW, &menu_items);
  }

  if (crostini::IsUninstallable(profile_, app_id)) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, &menu_items);
  }

  if (app_id == crostini::kCrostiniTerminalSystemAppId) {
    AddCommandItem(ash::SETTINGS, IDS_INTERNAL_APP_SETTINGS, &menu_items);
    if (crostini::IsCrostiniRunning(profile_)) {
      AddCommandItem(ash::SHUTDOWN_GUEST_OS,
                     IDS_CROSTINI_SHUT_DOWN_LINUX_MENU_ITEM, &menu_items);
    }
  }

  if (ShouldAddOpenItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_OPEN_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                   &menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, &menu_items);
  }

  // Offer users the ability to toggle per-application UI scaling.
  // Some apps have high-density display support and do not require scaling
  // to match the system display density, but others are density-unaware and
  // look better when scaled to match the display density.
  if (ShouldShowDisplayDensityMenuItem(app_id, menu_type, display_id)) {
    base::Optional<guest_os::GuestOsRegistryService::Registration>
        registration = registry_->GetRegistration(app_id);
    if (registration) {
      if (registration->IsScaled()) {
        AddCommandItem(ash::CROSTINI_USE_HIGH_DENSITY,
                       IDS_CROSTINI_USE_HIGH_DENSITY, &menu_items);
      } else {
        AddCommandItem(ash::CROSTINI_USE_LOW_DENSITY,
                       IDS_CROSTINI_USE_LOW_DENSITY, &menu_items);
      }
    }
  }

  std::move(callback).Run(std::move(menu_items));
}

void CrostiniApps::OnRegistryUpdated(
    guest_os::GuestOsRegistryService* registry_service,
    guest_os::GuestOsRegistryService::VmType vm_type,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  if (vm_type != guest_os::GuestOsRegistryService::VmType::
                     ApplicationList_VmType_TERMINA) {
    return;
  }

  for (const std::string& app_id : updated_apps) {
    if (auto registration = registry_->GetRegistration(app_id)) {
      Publish(Convert(*registration, /*new_icon_key=*/false), subscribers_);
    }
  }
  for (const std::string& app_id : removed_apps) {
    apps::mojom::AppPtr app = apps::mojom::App::New();
    app->app_type = apps::mojom::AppType::kCrostini;
    app->app_id = app_id;
    app->readiness = apps::mojom::Readiness::kUninstalledByUser;
    Publish(std::move(app), subscribers_);
  }
  for (const std::string& app_id : inserted_apps) {
    if (auto registration = registry_->GetRegistration(app_id)) {
      Publish(Convert(*registration, /*new_icon_key=*/true), subscribers_);
    }
  }
}

void CrostiniApps::OnCrostiniEnabledChanged() {
  crostini_enabled_ =
      profile_ && crostini::CrostiniFeatures::Get()->IsEnabled(profile_);
  auto show = crostini_enabled_ ? apps::mojom::OptionalBool::kTrue
                                : apps::mojom::OptionalBool::kFalse;

  // The Crostini Terminal app is a hard-coded special case. It is the entry
  // point to installing other Crostini apps, and is always in search.
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kCrostini;
  app->app_id = crostini::kCrostiniTerminalSystemAppId;
  app->show_in_launcher = show;
  app->show_in_shelf = show;
  app->show_in_search = apps::mojom::OptionalBool::kTrue;
  Publish(std::move(app), subscribers_);
}

apps::mojom::AppPtr CrostiniApps::Convert(
    const guest_os::GuestOsRegistryService::Registration& registration,
    bool new_icon_key) {
  DCHECK_EQ(
      registration.VmType(),
      guest_os::GuestOsRegistryService::VmType::ApplicationList_VmType_TERMINA);

  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      apps::mojom::AppType::kCrostini, registration.app_id(),
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
    app->icon_key = NewIconKey(registration.app_id());
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  auto show = apps::mojom::OptionalBool::kTrue;
  if (registration.NoDisplay()) {
    show = apps::mojom::OptionalBool::kFalse;
  }
  auto show_in_search = show;
  if (registration.app_id() == crostini::kCrostiniTerminalSystemAppId) {
    show = crostini_enabled_ ? apps::mojom::OptionalBool::kTrue
                             : apps::mojom::OptionalBool::kFalse;
    // The Crostini Terminal should appear in the app search, even when
    // Crostini is not installed.
    show_in_search = apps::mojom::OptionalBool::kTrue;
  }
  app->show_in_launcher = show;
  app->show_in_search = show_in_search;
  app->show_in_shelf = show_in_search;
  // TODO(crbug.com/955937): Enable once Crostini apps are managed inside App
  // Management.
  app->show_in_management = apps::mojom::OptionalBool::kFalse;

  return app;
}

apps::mojom::IconKeyPtr CrostiniApps::NewIconKey(const std::string& app_id) {
  DCHECK(!app_id.empty());

  // Treat the Crostini Terminal as a special case, loading an icon defined by
  // a resource instead of asking the Crostini VM (or the cache of previous
  // responses from the Crostini VM). Presumably this is for bootstrapping: the
  // Crostini Terminal icon (the UI for enabling and installing Crostini apps)
  // should be showable even before the user has installed their first Crostini
  // app and before bringing up an Crostini VM for the first time.
  if (app_id == crostini::kCrostiniTerminalSystemAppId) {
    return apps::mojom::IconKey::New(
        apps::mojom::IconKey::kDoesNotChangeOverTime,
        IDR_LOGO_CROSTINI_TERMINAL, apps::IconEffects::kNone);
  }

  auto icon_effects =
      base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)
          ? IconEffects::kCrOsStandardIcon
          : IconEffects::kNone;
  return icon_key_factory_.MakeIconKey(icon_effects);
}

}  // namespace apps

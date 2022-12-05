// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/crostini_apps.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_base.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_package_service.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/file_system/file_system_context.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"

// TODO(crbug.com/826982): the equivalent of
// CrostiniAppModelBuilder::MaybeCreateRootFolder. Does some sort of "root
// folder" abstraction belong here (on the publisher side of the App Service)
// or should we hard-code that in one particular subscriber (the App List UI)?

namespace {

const char kTextPlainMimeType[] = "text/plain";
const char kTextTypeMimeType[] = "text/";
const char kTextWildcardMimeType[] = "text/*";

bool ShouldShowDisplayDensityMenuItem(const std::string& app_id,
                                      apps::MenuType menu_type,
                                      int64_t display_id) {
  // The default terminal app is crosh in a Chrome window and it doesn't run in
  // the Crostini container so it doesn't support display density the same way.
  if (menu_type != apps::MenuType::kShelf) {
    return false;
  }

  display::Display d;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id, &d)) {
    return true;
  }

  return d.device_scale_factor() != 1.0;
}

// Create a file intent filter with mime type conditions for App Service.
apps::IntentFilters CreateIntentFilterForCrostini(
    const guest_os::GuestOsMimeTypesService* mime_types_service,
    const guest_os::GuestOsRegistryService::Registration& registration) {
  const std::set<std::string> mime_types_set = registration.MimeTypes();
  if (mime_types_set.empty()) {
    return {};
  }

  // When a file has a mime type that Files App can't recognise but Crostini can
  // (e.g. a proprietary file type), we should look at the file extensions that
  // the app can support. We find these extension types by checking what
  // extensions correspond to the app's supported mime types.
  std::vector<std::string> extension_types;
  if (ash::features::ShouldArcAndGuestOsFileTasksUseAppService()) {
    extension_types = mime_types_service->GetExtensionTypesFromMimeTypes(
        mime_types_set, registration.VmName(), registration.ContainerName());
  }
  std::vector<std::string> mime_types(mime_types_set.begin(),
                                      mime_types_set.end());

  // If we see that the app supports the text/plain mime-type, then the app
  // supports all files with type text/*, as per xdg spec.
  // https://specifications.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-latest.html.
  // In this case, remove all mime types that begin with "text/" and replace
  // them with a single "text/*" mime type.
  if (base::Contains(mime_types, kTextPlainMimeType)) {
    mime_types.erase(std::remove_if(mime_types.begin(), mime_types.end(),
                                    [](const std::string& mime) {
                                      return mime.find(kTextTypeMimeType) !=
                                             std::string::npos;
                                    }),
                     mime_types.end());
    mime_types.push_back(kTextWildcardMimeType);
  }

  apps::IntentFilters intent_filters;
  intent_filters.push_back(apps_util::CreateFileFilter(
      {apps_util::kIntentActionView}, mime_types, extension_types,
      // TODO(crbug/1349974): Remove activity_name when default file handling
      // preferences for Files App are migrated.
      /*activity_name=*/apps_util::kGuestOsActivityName));

  return intent_filters;
}

}  // namespace

namespace apps {

CrostiniApps::CrostiniApps(AppServiceProxy* proxy)
    : AppPublisher(proxy), profile_(proxy->profile()) {}

CrostiniApps::~CrostiniApps() {
  if (registry_) {
    registry_->RemoveObserver(this);
  }
}

void CrostiniApps::Initialize() {
  DCHECK(profile_);
  if (!crostini::CrostiniFeatures::Get()->CouldBeAllowed(profile_)) {
    return;
  }
  registry_ = guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_);
  if (!registry_) {
    return;
  }

  registry_->AddObserver(this);

  PublisherBase::Initialize(proxy()->AppService(),
                            apps::mojom::AppType::kCrostini);

  RegisterPublisher(AppType::kCrostini);

  std::vector<AppPtr> apps;
  for (const auto& pair :
       registry_->GetRegisteredApps(guest_os::VmType::TERMINA)) {
    const guest_os::GuestOsRegistryService::Registration& registration =
        pair.second;
    apps.push_back(CreateApp(registration, /*generate_new_icon_key=*/true));
  }
  AppPublisher::Publish(std::move(apps), AppType::kCrostini,
                        /*should_notify_initialized=*/true);
}

void CrostiniApps::LoadIcon(const std::string& app_id,
                            const IconKey& icon_key,
                            IconType icon_type,
                            int32_t size_hint_in_dip,
                            bool allow_placeholder_icon,
                            apps::LoadIconCallback callback) {
  registry_->LoadIcon(app_id, icon_key, icon_type, size_hint_in_dip,
                      allow_placeholder_icon, IDR_LOGO_CROSTINI_DEFAULT,
                      std::move(callback));
}

void CrostiniApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          LaunchSource launch_source,
                          WindowInfoPtr window_info) {
  crostini::LaunchCrostiniApp(
      profile_, app_id,
      window_info ? window_info->display_id : display::kInvalidDisplayId);
}

void CrostiniApps::LaunchAppWithIntent(const std::string& app_id,
                                       int32_t event_flags,
                                       IntentPtr intent,
                                       LaunchSource launch_source,
                                       WindowInfoPtr window_info,
                                       LaunchCallback callback) {
  // Retrieve URLs from the files in the intent.
  std::vector<crostini::LaunchArg> args;
  if (intent && intent->files.size() > 0) {
    args.reserve(intent->files.size());
    storage::FileSystemContext* file_system_context =
        file_manager::util::GetFileManagerFileSystemContext(profile_);
    for (auto& file : intent->files) {
      args.emplace_back(
          file_system_context->CrackURLInFirstPartyContext(file->url));
    }
  }
  crostini::LaunchCrostiniAppWithIntent(
      profile_, app_id,
      window_info ? window_info->display_id : display::kInvalidDisplayId,
      std::move(intent), args,
      base::BindOnce(
          [](LaunchCallback callback, bool success,
             const std::string& failure_reason) {
            if (!success) {
              LOG(ERROR) << "Crostini launch error: " << failure_reason;
            }
            std::move(callback).Run(ConvertBoolToLaunchResult(success));
          },
          std::move(callback)));
}

void CrostiniApps::LaunchAppWithParams(AppLaunchParams&& params,
                                       LaunchCallback callback) {
  auto event_flags = apps::GetEventFlags(params.disposition,
                                         /*prefer_container=*/false);
  if (params.intent) {
    LaunchAppWithIntent(params.app_id, event_flags, std::move(params.intent),
                        params.launch_source,
                        std::make_unique<WindowInfo>(params.display_id),
                        std::move(callback));
  } else {
    Launch(params.app_id, event_flags, params.launch_source,
           std::make_unique<WindowInfo>(params.display_id));
    // TODO(crbug.com/1244506): Add launch return value.
    std::move(callback).Run(LaunchResult());
  }
}

void CrostiniApps::Uninstall(const std::string& app_id,
                             UninstallSource uninstall_source,
                             bool clear_site_data,
                             bool report_abuse) {
  crostini::CrostiniPackageService::GetForProfile(profile_)
      ->QueueUninstallApplication(app_id);
}

void CrostiniApps::GetMenuModel(const std::string& app_id,
                                MenuType menu_type,
                                int64_t display_id,
                                base::OnceCallback<void(MenuItems)> callback) {
  MenuItems menu_items;

  if (menu_type == MenuType::kShelf) {
    AddCommandItem(ash::APP_CONTEXT_MENU_NEW_WINDOW, IDS_APP_LIST_NEW_WINDOW,
                   menu_items);
  }

  if (crostini::IsUninstallable(profile_, app_id)) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, menu_items);
  }

  if (ShouldAddOpenItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::LAUNCH_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                   menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, menu_items);
  }

  // Offer users the ability to toggle per-application UI scaling.
  // Some apps have high-density display support and do not require scaling
  // to match the system display density, but others are density-unaware and
  // look better when scaled to match the display density.
  if (ShouldShowDisplayDensityMenuItem(app_id, menu_type, display_id)) {
    absl::optional<guest_os::GuestOsRegistryService::Registration>
        registration = registry_->GetRegistration(app_id);
    if (registration) {
      if (registration->IsScaled()) {
        AddCommandItem(ash::CROSTINI_USE_HIGH_DENSITY,
                       IDS_CROSTINI_USE_HIGH_DENSITY, menu_items);
      } else {
        AddCommandItem(ash::CROSTINI_USE_LOW_DENSITY,
                       IDS_CROSTINI_USE_LOW_DENSITY, menu_items);
      }
    }
  }

  std::move(callback).Run(std::move(menu_items));
}

void CrostiniApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;

  for (const auto& pair :
       registry_->GetRegisteredApps(guest_os::VmType::TERMINA)) {
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

void CrostiniApps::OnRegistryUpdated(
    guest_os::GuestOsRegistryService* registry_service,
    guest_os::VmType vm_type,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  if (vm_type != guest_os::VmType::TERMINA) {
    return;
  }

  // TODO(sidereal) Do something cleverer here so we only need to publish a new
  // icon when the icon has actually changed.
  const bool update_icon =
      crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(profile_);
  for (const std::string& app_id : updated_apps) {
    if (auto registration = registry_->GetRegistration(app_id)) {
      PublisherBase::Publish(
          Convert(*registration, /*new_icon_key=*/update_icon), subscribers_);
      AppPublisher::Publish(
          CreateApp(*registration, /*generate_new_icon_key=*/update_icon));
    }
  }
  for (const std::string& app_id : removed_apps) {
    apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
    mojom_app->app_type = apps::mojom::AppType::kCrostini;
    mojom_app->app_id = app_id;
    mojom_app->readiness = apps::mojom::Readiness::kUninstalledByUser;
    PublisherBase::Publish(std::move(mojom_app), subscribers_);

    auto app = std::make_unique<App>(AppType::kCrostini, app_id);
    app->readiness = Readiness::kUninstalledByUser;
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

AppPtr CrostiniApps::CreateApp(
    const guest_os::GuestOsRegistryService::Registration& registration,
    bool generate_new_icon_key) {
  DCHECK_EQ(registration.VmType(), guest_os::VmType::TERMINA);

  auto app = AppPublisher::MakeApp(
      AppType::kCrostini, registration.app_id(), Readiness::kReady,
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

  auto show = !registration.NoDisplay();
  app->show_in_launcher = show;
  app->show_in_search = show;
  app->show_in_shelf = show;
  // TODO(crbug.com/955937): Enable once Crostini apps are managed inside App
  // Management.
  app->show_in_management = false;

  app->allow_uninstall =
      crostini::IsUninstallable(profile_, registration.app_id());

  app->handles_intents = true;

  const guest_os::GuestOsMimeTypesService* mime_types_service =
      guest_os::GuestOsMimeTypesServiceFactory::GetForProfile(profile_);

  app->intent_filters =
      CreateIntentFilterForCrostini(mime_types_service, registration);

  // TODO(crbug.com/1253250): Add other fields for the App struct.
  return app;
}

apps::mojom::AppPtr CrostiniApps::Convert(
    const guest_os::GuestOsRegistryService::Registration& registration,
    bool new_icon_key) {
  DCHECK_EQ(registration.VmType(), guest_os::VmType::TERMINA);

  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      apps::mojom::AppType::kCrostini, registration.app_id(),
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
    app->icon_key = NewIconKey(registration.app_id());
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  auto show = apps::mojom::OptionalBool::kTrue;
  if (registration.NoDisplay()) {
    show = apps::mojom::OptionalBool::kFalse;
  }
  app->show_in_launcher = show;
  app->show_in_search = show;
  app->show_in_shelf = show;
  // TODO(crbug.com/955937): Enable once Crostini apps are managed inside App
  // Management.
  app->show_in_management = apps::mojom::OptionalBool::kFalse;

  app->allow_uninstall =
      crostini::IsUninstallable(profile_, registration.app_id())
          ? apps::mojom::OptionalBool::kTrue
          : apps::mojom::OptionalBool::kFalse;

  app->handles_intents = apps::mojom::OptionalBool::kTrue;

  const guest_os::GuestOsMimeTypesService* mime_types_service =
      guest_os::GuestOsMimeTypesServiceFactory::GetForProfile(profile_);

  app->intent_filters = ConvertIntentFiltersToMojomIntentFilters(
      CreateIntentFilterForCrostini(mime_types_service, registration));

  return app;
}

apps::mojom::IconKeyPtr CrostiniApps::NewIconKey(const std::string& app_id) {
  DCHECK(!app_id.empty());
  auto icon_effects = IconEffects::kCrOsStandardIcon;
  return icon_key_factory_.MakeIconKey(icon_effects);
}

}  // namespace apps

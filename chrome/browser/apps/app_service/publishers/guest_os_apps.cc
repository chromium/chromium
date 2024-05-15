// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/guest_os_apps.h"

#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_base.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "storage/browser/file_system/file_system_context.h"

namespace apps {

GuestOSApps::GuestOSApps(AppServiceProxy* proxy)
    : AppPublisher(proxy), profile_(proxy->profile()) {}

GuestOSApps::~GuestOSApps() = default;

void GuestOSApps::InitializeForTesting() {
  CHECK_IS_TEST();
  Initialize();
}

void GuestOSApps::Initialize() {
  DCHECK(profile_);
  if (!CouldBeAllowed()) {
    // Set the publisher unavailable to remove apps saved in the AppStorage
    // file, and related launch requests.
    proxy()->SetPublisherUnavailable(AppType());
    return;
  }
  registry_ = guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_);
  if (!registry_) {
    // Set the publisher unavailable to remove apps saved in the AppStorage
    // file, and related launch requests.
    proxy()->SetPublisherUnavailable(AppType());
    return;
  }
  registry_observation_.Observe(registry_);
  RegisterPublisher(AppType());
  std::vector<AppPtr> apps;
  for (const auto& pair : registry_->GetRegisteredApps(VmType())) {
    const guest_os::GuestOsRegistryService::Registration& registration =
        pair.second;
    apps.push_back(CreateApp(registration,
                             /*generate_new_icon_key=*/true));
  }
  AppPublisher::Publish(std::move(apps), AppType(),
                        /*should_notify_initialized=*/true);
}

std::vector<guest_os::LaunchArg> GuestOSApps::ArgsFromIntent(
    const apps::Intent* intent) {
  std::vector<guest_os::LaunchArg> args;
  if (!intent || intent->files.empty()) {
    return args;
  }
  args.reserve(intent->files.size());
  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile());
  for (auto& file : intent->files) {
    args.emplace_back(
        file_system_context->CrackURLInFirstPartyContext(file->url));
  }
  return args;
}

void GuestOSApps::GetCompressedIconData(const std::string& app_id,
                                        int32_t size_in_dip,
                                        ui::ResourceScaleFactor scale_factor,
                                        LoadIconCallback callback) {
  GetGuestOSAppCompressedIconData(profile_, app_id, size_in_dip, scale_factor,
                                  std::move(callback));
}

void GuestOSApps::LaunchAppWithParams(AppLaunchParams&& params,
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
    // TODO(crbug.com/40787924): Add launch return value.
    std::move(callback).Run(LaunchResult());
  }
}

void GuestOSApps::OnRegistryUpdated(
    guest_os::GuestOsRegistryService* registry_service,
    guest_os::VmType vm_type,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  if (vm_type != VmType()) {
    return;
  }
  // TODO(sidereal): Do something cleverer here so we only need to publish a new
  // icon when the icon has actually changed.
  for (const std::string& app_id : updated_apps) {
    if (auto registration = registry_->GetRegistration(app_id)) {
      AppPublisher::Publish(
          CreateApp(*registration, /*generate_new_icon_key=*/true));
    }
  }
  for (const std::string& app_id : removed_apps) {
    auto app = std::make_unique<App>(AppType(), app_id);
    app->readiness = Readiness::kUninstalledByUser;
    AppPublisher::Publish(std::move(app));
  }
  for (const std::string& app_id : inserted_apps) {
    if (auto registration = registry_->GetRegistration(app_id)) {
      AppPublisher::Publish(
          CreateApp(*registration, /*generate_new_icon_key=*/true));
    }
  }
}

void GuestOSApps::OnAppLastLaunchTimeUpdated(
    guest_os::VmType vm_type,
    const std::string& app_id,
    const base::Time& last_launch_time) {
  if (vm_type != VmType()) {
    return;
  }

  auto app = std::make_unique<App>(AppType(), app_id);
  app->last_launch_time = last_launch_time;
  std::vector<AppPtr> apps;
  apps.push_back(std::move(app));
  AppPublisher::Publish(std::move(apps), AppType(),
                        /*should_notify_initialized=*/false);
}

AppPtr GuestOSApps::CreateApp(
    const guest_os::GuestOsRegistryService::Registration& registration,
    bool generate_new_icon_key) {
  DCHECK_EQ(registration.VmType(), VmType());
  auto app = AppPublisher::MakeApp(
      AppType(), registration.app_id(), Readiness::kReady, registration.Name(),
      InstallReason::kUser, InstallSource::kUnknown);

  const std::string& executable_file_name = registration.ExecutableFileName();
  if (!executable_file_name.empty()) {
    app->additional_search_terms.push_back(executable_file_name);
  }
  for (const std::string& keyword : registration.Keywords()) {
    app->additional_search_terms.push_back(keyword);
  }

  if (generate_new_icon_key) {
    IconEffects icon_effects = IconEffects::kCrOsStandardIcon;
    if (crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(profile_)) {
      icon_effects |= IconEffects::kGuestOsBadge;
    }
    app->icon_key = IconKey(icon_effects);
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  auto show = !registration.NoDisplay();
  app->show_in_launcher = show;
  app->show_in_search = show;
  app->show_in_shelf = show;
  app->show_in_management = false;
  app->allow_uninstall = false;
  app->allow_close = true;

  // Add intent filters based on file extensions.
  app->handles_intents = true;
  const guest_os::GuestOsMimeTypesService* mime_types_service =
      guest_os::GuestOsMimeTypesServiceFactory::GetForProfile(profile());
  app->intent_filters =
      CreateIntentFilterForAppService(mime_types_service, registration);

  app->SetExtraField("vm_name", registration.VmName());
  app->SetExtraField("container_name", registration.ContainerName());
  app->SetExtraField("desktop_file_id", registration.DesktopFileId());
  app->SetExtraField("exec", registration.Exec());
  app->SetExtraField("executable_file_name", registration.ExecutableFileName());
  app->SetExtraField("no_display", registration.NoDisplay());
  app->SetExtraField("terminal", registration.Terminal());
  app->SetExtraField("scaled", registration.IsScaled());
  app->SetExtraField("package_id", registration.PackageId());
  app->SetExtraField("startup_wm_class", registration.StartupWmClass());
  app->SetExtraField("startup_notify", registration.StartupNotify());

  // Allow subclasses of GuestOSApps to modify app.
  CreateAppOverrides(registration, app.get());

  return app;
}

apps::IntentFilters CreateIntentFilterForAppService(
    const guest_os::GuestOsMimeTypesService* mime_types_service,
    const guest_os::GuestOsRegistryService::Registration& registration) {
  const std::set<std::string> mime_types_set = registration.MimeTypes();
  if (mime_types_set.empty()) {
    return {};
  }

  // When a file has a mime type that Files App can't recognise but the guest
  // can (e.g. a proprietary file type), we should look at the file extensions
  // that the app can support. We find these extension types by checking what
  // extensions correspond to the app's supported mime types.
  std::vector<std::string> extension_types =
      mime_types_service->GetExtensionTypesFromMimeTypes(
          mime_types_set, registration.VmName(), registration.ContainerName());
  std::vector<std::string> mime_types(mime_types_set.begin(),
                                      mime_types_set.end());

  // If we see that the app supports the text/plain mime-type, then the app
  // supports all files with type text/*, as per xdg spec.
  // https://specifications.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-latest.html.
  // In this case, remove all mime types that begin with "text/" and replace
  // them with a single "text/*" mime type.
  if (base::Contains(mime_types, "text/plain")) {
    std::erase_if(mime_types, [](const std::string& s) {
      return base::StartsWith(s, "text/");
    });
    mime_types.push_back("text/*");
  }

  apps::IntentFilters intent_filters;
  intent_filters.push_back(apps_util::CreateFileFilter(
      {apps_util::kIntentActionView}, mime_types, extension_types,
      // TODO(crbug.com/40233967): Remove activity_name when default file
      // handling preferences for Files App are migrated.
      /*activity_name=*/apps_util::kGuestOsActivityName));

  return intent_filters;
}

}  // namespace apps

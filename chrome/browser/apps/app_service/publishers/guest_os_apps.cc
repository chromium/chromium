// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/guest_os_apps.h"

#include <utility>
#include <vector>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_base.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

GuestOSApps::GuestOSApps(AppServiceProxy* proxy)
    : AppPublisher(proxy), profile_(proxy->profile()) {}

GuestOSApps::~GuestOSApps() = default;

void GuestOSApps::Initialize() {
  DCHECK(profile_);
  if (!CouldBeAllowed()) {
    return;
  }
  registry_ = guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_);
  if (!registry_) {
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

void GuestOSApps::GetCompressedIconData(const std::string& app_id,
                                        int32_t size_in_dip,
                                        ui::ResourceScaleFactor scale_factor,
                                        LoadIconCallback callback) {
  GetGuestOSAppCompressedIconData(profile_, app_id, size_in_dip, scale_factor,
                                  std::move(callback));
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
    app->icon_key = std::move(
        *icon_key_factory_.CreateIconKey(IconEffects::kCrOsStandardIcon));
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  auto show = !registration.NoDisplay();
  app->show_in_launcher = show;
  app->show_in_search = show;
  app->show_in_shelf = show;
  app->show_in_management = false;
  app->allow_uninstall = false;
  app->handles_intents = false;

  // Allow subclasses of GuestOSApps to modify app.
  CreateAppOverrides(registration, app.get());

  return app;
}

}  // namespace apps

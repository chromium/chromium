// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/bruschetta_apps.h"

#include <utility>
#include <vector>

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/bruschetta/bruschetta_features.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

BruschettaApps::BruschettaApps(AppServiceProxy* proxy) : GuestOSApps(proxy) {}

bool BruschettaApps::CouldBeAllowed() const {
  return bruschetta::BruschettaFeatures::Get()->IsEnabled();
}

apps::AppType BruschettaApps::AppType() const {
  return AppType::kBruschetta;
}

guest_os::VmType BruschettaApps::VmType() const {
  return guest_os::VmType::BRUSCHETTA;
}

void BruschettaApps::LoadIcon(const std::string& app_id,
                              const IconKey& icon_key,
                              IconType icon_type,
                              int32_t size_hint_in_dip,
                              bool allow_placeholder_icon,
                              apps::LoadIconCallback callback) {
  // TODO(b/247636749): Consider creating IDR_LOGO_BRUSCHETTA_DEFAULT
  // to replace IconKey::kInvalidResourceId.
  registry()->LoadIcon(app_id, icon_key, icon_type, size_hint_in_dip,
                       allow_placeholder_icon, IconKey::kInvalidResourceId,
                       std::move(callback));
}

void BruschettaApps::Launch(const std::string& app_id,
                            int32_t event_flags,
                            LaunchSource launch_source,
                            WindowInfoPtr window_info) {
  // TODO(b/247636749): Implement this.
}

void BruschettaApps::LaunchAppWithParams(AppLaunchParams&& params,
                                         LaunchCallback callback) {
  // TODO(b/247636749): Implement this.
}

void BruschettaApps::CreateAppOverrides(
    const guest_os::GuestOsRegistryService::Registration& registration,
    App* app) {
  // TODO(b/247636749): Implement IsUninstallable and use it here.
  // TODO(b/247636749): Implement intent filter and use it here.
  // TODO(crbug.com/1253250): Add other fields for the App struct.
}

}  // namespace apps

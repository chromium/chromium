// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_types.h"

#include <ostream>
#include <utility>

namespace apps {

// Do not modify existing strings, they are used by metrics.
std::ostream& operator<<(std::ostream& out, AppInstallSurface surface) {
  switch (surface) {
    case AppInstallSurface::kAppPreloadServiceOem:
      return out << "AppPreloadServiceOem";
    case AppInstallSurface::kAppPreloadServiceDefault:
      return out << "AppPreloadServiceDefault";
    case AppInstallSurface::kOobeAppRecommendations:
      return out << "OobeAppRecommendations";
    case AppInstallSurface::kAppInstallUriUnknown:
      return out << "AppInstallUriUnknown";
    case AppInstallSurface::kAppInstallUriShowoff:
      return out << "AppInstallUriShowoff";
    case AppInstallSurface::kAppInstallUriMall:
      return out << "AppInstallUriMall";
    case AppInstallSurface::kAppInstallUriMallV2:
      return out << "AppInstallUriMallV2";
    case AppInstallSurface::kAppInstallUriGetit:
      return out << "AppInstallUriGetit";
    case AppInstallSurface::kAppInstallUriLauncher:
      return out << "AppInstallUriLauncher";
    case AppInstallSurface::kAppInstallUriPeripherals:
      return out << "AppInstallUriPeripherals";
  }
}

std::ostream& operator<<(std::ostream& out, const AppInstallIcon& icon) {
  out << "AppInstallIcon{";
  out << "url: " << icon.url;
  out << ", width_in_pixels: " << icon.width_in_pixels;
  out << ", mime_type: " << icon.mime_type;
  out << ", is_masking_allowed: " << icon.is_masking_allowed;
  return out << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AppInstallScreenshot& screenshot) {
  out << "AppInstallScreenshot{";
  out << "url: " << screenshot.url;
  out << ", mime_type: " << screenshot.mime_type;
  out << ", width_in_pixels: " << screenshot.width_in_pixels;
  out << ", height_in_pixels: " << screenshot.height_in_pixels;
  return out << "}";
}

std::ostream& operator<<(std::ostream& out, const AndroidAppInstallData& data) {
  return out << "AndroidAppInstallData{}";
}

WebAppInstallData::WebAppInstallData() = default;
WebAppInstallData::WebAppInstallData(const WebAppInstallData&) = default;
WebAppInstallData::WebAppInstallData(WebAppInstallData&&) = default;
WebAppInstallData& WebAppInstallData::operator=(const WebAppInstallData&) =
    default;
WebAppInstallData& WebAppInstallData::operator=(WebAppInstallData&&) = default;

WebAppInstallData::~WebAppInstallData() = default;

std::ostream& operator<<(std::ostream& out, const WebAppInstallData& data) {
  out << "WebAppInstallData{";
  out << ", original_manifest_url: " << data.original_manifest_url;
  out << ", proxied_manifest_url: " << data.proxied_manifest_url;
  out << ", document_url: " << data.document_url;
  out << ", open_as_window: " << data.open_as_window;
  return out << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const GeForceNowAppInstallData& data) {
  return out << "GeForceNowAppInstallData{}";
}

std::ostream& operator<<(std::ostream& out, const SteamAppInstallData& data) {
  return out << "SteamAppInstallData{}";
}

AppInstallData::AppInstallData(PackageId package_id)
    : package_id(std::move(package_id)) {}

AppInstallData::AppInstallData(const AppInstallData&) = default;
AppInstallData& AppInstallData::operator=(const AppInstallData&) = default;
AppInstallData::AppInstallData(AppInstallData&&) = default;
AppInstallData& AppInstallData::operator=(AppInstallData&&) = default;

AppInstallData::~AppInstallData() = default;

bool AppInstallData::IsValidForInstallation() const {
  if (package_id.package_type() == PackageType::kWeb ||
      package_id.package_type() == PackageType::kWebsite) {
    if (!absl::holds_alternative<WebAppInstallData>(app_type_data)) {
      return false;
    }
  } else if (!install_url.is_valid()) {
    // For all package types other than Web/Website, there must be an Install
    // URL for us to launch.
    return false;
  }

  return true;
}

std::ostream& operator<<(std::ostream& out, const AppInstallData& data) {
  out << "AppInstallData{";

  out << "package_id: " << data.package_id.ToString();

  out << ", name: " << data.name;

  out << ", description: " << data.description;

  if (data.icon.has_value()) {
    out << ", icon: " << data.icon.value();
  }

  out << ", screenshots: {";
  for (const AppInstallScreenshot& screenshot : data.screenshots) {
    out << screenshot << ", ";
  }
  out << "}, ";

  out << ", install_url: " << data.install_url;

  out << ", app_type_data: ";
  absl::visit([&out](const auto& data) { out << data; }, data.app_type_data);

  return out << "}";
}

}  // namespace apps

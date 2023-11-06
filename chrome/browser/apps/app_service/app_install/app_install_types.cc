// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_types.h"

#include <ostream>
#include <utility>

namespace apps {

std::ostream& operator<<(std::ostream& out, const AppInstallIcon& icon) {
  out << "AppInstallIcon{";
  out << "url: " << icon.url;
  out << ", width_in_pixels: " << icon.width_in_pixels;
  out << ", mime_type: " << icon.mime_type;
  out << ", is_masking_allowed: " << icon.is_masking_allowed;
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
  out << "manifest_id: " << data.manifest_id;
  out << ", original_manifest_url: " << data.original_manifest_url;
  out << ", proxied_manifest_url: " << data.proxied_manifest_url;
  out << ", document_url: " << data.document_url;
  return out << "}";
}

AppInstallData::AppInstallData(PackageId package_id)
    : package_id(std::move(package_id)) {}

AppInstallData::AppInstallData(const AppInstallData&) = default;
AppInstallData& AppInstallData::operator=(const AppInstallData&) = default;
AppInstallData::AppInstallData(AppInstallData&&) = default;
AppInstallData& AppInstallData::operator=(AppInstallData&&) = default;

AppInstallData::~AppInstallData() = default;

std::ostream& operator<<(std::ostream& out, const AppInstallData& data) {
  out << "AppInstallData{";

  out << "package_id: " << data.package_id.ToString();

  out << ", name: " << data.name;

  out << ", description: " << data.description;

  out << ", icons: {";
  for (const AppInstallIcon& icon : data.icons) {
    out << icon << ", ";
  }
  out << "}, ";

  out << ", app_type_data: ";
  absl::visit([&out](const auto& data) { out << data; }, data.app_type_data);

  return out << "}";
}

}  // namespace apps

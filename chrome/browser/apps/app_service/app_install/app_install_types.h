// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_TYPES_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_TYPES_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "components/services/app_service/public/cpp/package_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace apps {

// App icons hosted by Almanac for use during app installation.
struct AppInstallIcon {
  GURL url;

  int32_t width_in_pixels;

  std::string mime_type;

  bool is_masking_allowed;
};

std::ostream& operator<<(std::ostream& out, const AppInstallIcon& data);

// Android specific data for use during Android app installation.
// Currently empty but available to be extended with data if needed.
struct AndroidAppInstallData {};

std::ostream& operator<<(std::ostream& out, const AndroidAppInstallData& data);

// Web app specific data for use during web app installation.
struct WebAppInstallData {
  WebAppInstallData();
  WebAppInstallData(const WebAppInstallData&);
  WebAppInstallData(WebAppInstallData&&);
  WebAppInstallData& operator=(const WebAppInstallData&);
  WebAppInstallData& operator=(WebAppInstallData&&);
  ~WebAppInstallData();

  GURL manifest_id;

  GURL original_manifest_url;

  GURL proxied_manifest_url;

  GURL document_url;
};

std::ostream& operator<<(std::ostream& out, const WebAppInstallData& data);

// Generic app metadata for use in dialogs during app installation plus any app
// type specific information necessary for performing the app installation.
struct AppInstallData {
  explicit AppInstallData(PackageId package_id);
  AppInstallData(const AppInstallData&);
  AppInstallData(AppInstallData&&);
  AppInstallData& operator=(const AppInstallData&);
  AppInstallData& operator=(AppInstallData&&);
  ~AppInstallData();

  PackageId package_id;

  std::string name;

  std::string description;

  std::vector<AppInstallIcon> icons;

  absl::variant<AndroidAppInstallData, WebAppInstallData> app_type_data;
};

std::ostream& operator<<(std::ostream& out, const AppInstallData& data);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_TYPES_H_

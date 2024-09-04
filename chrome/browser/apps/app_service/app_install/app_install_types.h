// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_TYPES_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_TYPES_H_

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "components/services/app_service/public/cpp/package_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace apps {

// Additions to this enum must also update the
// AppInstallSurface variants in:
// tools/metrics/histograms/metadata/apps/histograms.xml
enum class AppInstallSurface {
  kAppPreloadServiceOem,
  kAppPreloadServiceDefault,
  kOobeAppRecommendations,

  // kAppInstallUri* values are not trustworthy, no decision making should
  // depend on these values.
  kAppInstallUriUnknown,
  kAppInstallUriShowoff,
  kAppInstallUriMall,
  kAppInstallUriMallV2,
  kAppInstallUriGetit,
  kAppInstallUriLauncher,
  kAppInstallUriPeripherals,
};

std::ostream& operator<<(std::ostream& out, AppInstallSurface surface);

// App icons hosted by Almanac for use during app installation.
struct AppInstallIcon {
  GURL url;

  int32_t width_in_pixels;

  std::string mime_type;

  bool is_masking_allowed;
};

std::ostream& operator<<(std::ostream& out, const AppInstallIcon& icon);

// App screenshots hosted by Almanac for use during app installation.
struct AppInstallScreenshot {
  GURL url;

  std::string mime_type;

  int32_t width_in_pixels;

  int32_t height_in_pixels;
};

std::ostream& operator<<(std::ostream& out,
                         const AppInstallScreenshot& screenshot);

// Android specific data for use during Android app installation.
// Currently empty but available to be extended with data if needed.
struct AndroidAppInstallData {};

std::ostream& operator<<(std::ostream& out, const AndroidAppInstallData& data);

// Web app specific data for use during web app installation.
// AppInstallData::package_id.identifier() holds the manifest identity.
struct WebAppInstallData {
  WebAppInstallData();
  WebAppInstallData(const WebAppInstallData&);
  WebAppInstallData(WebAppInstallData&&);
  WebAppInstallData& operator=(const WebAppInstallData&);
  WebAppInstallData& operator=(WebAppInstallData&&);
  ~WebAppInstallData();

  GURL original_manifest_url;

  GURL proxied_manifest_url;

  GURL document_url;

  // Ony used by PackageType::kWebsite shortcuts, to control whether the
  // shortcut opens in a browser tab or window. PackageType::kWeb apps will
  // ignore this value and always open in a window.
  bool open_as_window = false;
};

std::ostream& operator<<(std::ostream& out, const WebAppInstallData& data);

// GeForce Now specific data for use during GeForce Now app installation.
// Currently empty but available to be extended with data if needed.
struct GeForceNowAppInstallData {};

std::ostream& operator<<(std::ostream& out,
                         const GeForceNowAppInstallData& data);

// Steam specific data for use during Steam app installation.
// Currently empty but available to be extended with data if needed.
struct SteamAppInstallData {};

std::ostream& operator<<(std::ostream& out, const SteamAppInstallData& data);

// Generic app metadata for use in dialogs during app installation plus any app
// type specific information necessary for performing the app installation.
struct AppInstallData {
  explicit AppInstallData(PackageId package_id);
  AppInstallData(const AppInstallData&);
  AppInstallData(AppInstallData&&);
  AppInstallData& operator=(const AppInstallData&);
  AppInstallData& operator=(AppInstallData&&);
  ~AppInstallData();

  // Returns true if the data contains all the fields needed for installation,
  // dependent on the PackageId and `app_type_data`.
  bool IsValidForInstallation() const;

  PackageId package_id;

  std::string name;

  std::string description;

  std::optional<AppInstallIcon> icon;

  std::vector<AppInstallScreenshot> screenshots;

  GURL install_url;

  absl::variant<AndroidAppInstallData,
                WebAppInstallData,
                GeForceNowAppInstallData,
                SteamAppInstallData>
      app_type_data;
};

std::ostream& operator<<(std::ostream& out, const AppInstallData& data);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_TYPES_H_

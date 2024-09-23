// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_PRELOAD_APP_DEFINITION_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_PRELOAD_APP_DEFINITION_H_

#include <ostream>
#include <string>

#include "chrome/browser/apps/app_preload_service/proto/app_preload.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class GURL;

namespace apps {

// A wrapper class around an App Preload Server App proto to allow for easier
// extraction and conversion of information.
class PreloadAppDefinition {
 public:
  explicit PreloadAppDefinition(proto::AppPreloadListResponse_App app_proto);
  PreloadAppDefinition(const PreloadAppDefinition&);
  PreloadAppDefinition& operator=(const PreloadAppDefinition&);
  ~PreloadAppDefinition();

  std::optional<PackageId> GetPackageId() const;
  std::string GetName() const;
  PackageType GetPlatform() const;
  bool IsDefaultApp() const;
  bool IsOemApp() const;
  bool IsTestApp() const;
  AppInstallSurface GetInstallSurface() const;

  // Returns the android package name. This is derived from the package
  // identifier of the app. Must only be called if `GetPlatform()` returns
  // `AppType::kArc`.
  std::string GetAndroidPackageName() const;

  // Returns the Web App manifest URL for the app, which hosts the manifest of
  // the app in a JSON format. The URL could point to a local file, or a web
  // address. Does not attempt to validate the GURL. Must only be called if
  // `GetPlatform()` returns `AppType::kWeb`.
  GURL GetWebAppManifestUrl() const;

  // Returns the original Web App manifest URL for the app. This is the URL
  // where the manifest was originally hosted. Does not attempt to validate the
  // GURL. Must only be called if `GetPlatform()` returns `AppType::kWeb`.
  GURL GetWebAppOriginalManifestUrl() const;

  // Returns the manifest ID of the Web App. This is derived from the package
  // identifier of the app. Does not attempt to validate the GURL. Must only be
  // called if `GetPlatform()` returns `AppType::kWeb`.
  GURL GetWebAppManifestId() const;

  AppInstallData ToAppInstallData() const;

 private:
  proto::AppPreloadListResponse_App app_proto_;
  std::optional<apps::PackageId> package_id_;
};

std::ostream& operator<<(std::ostream& os, const PreloadAppDefinition& app);

// Wrapper for App Preload Server ShelfConfig proto. Map of PackageId to order.
using ShelfPinOrdering = std::map<apps::PackageId, uint32_t>;

// Wrappers for App Preload Server LauncherConfig proto:

// LauncherItem is either an app represented by a PackageId, or a folder
// represented by a string.
using LauncherItem = absl::variant<apps::PackageId, std::string>;

// LauncherItemData is the associated data for a LauncherItem.
struct LauncherItemData {
  proto::AppPreloadListResponse_LauncherType type;
  uint32_t order;
};

// Map of LauncherItem to LauncherItemData.
using LauncherItemMap = std::map<LauncherItem, LauncherItemData>;

// Map of folder to LauncherItemMap.  Root folder always exists and is keyed by
// empty string.
using LauncherOrdering = std::map<std::string, LauncherItemMap>;

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_PRELOAD_APP_DEFINITION_H_

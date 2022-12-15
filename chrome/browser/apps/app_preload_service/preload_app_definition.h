// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_PRELOAD_APP_DEFINITION_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_PRELOAD_APP_DEFINITION_H_

#include <ostream>
#include <string>

#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"

class GURL;

namespace apps {

// A wrapper class around an App Preload Server proto to allow for easier
// extraction and conversion of information.
class PreloadAppDefinition {
 public:
  explicit PreloadAppDefinition(
      proto::AppProvisioningListAppsResponse_App app_proto)
      : app_proto_(app_proto) {}
  PreloadAppDefinition(const PreloadAppDefinition&) = default;
  PreloadAppDefinition& operator=(const PreloadAppDefinition&) = default;
  ~PreloadAppDefinition() = default;

  std::string GetName() const;
  AppType GetPlatform() const;
  bool IsOemApp() const;

  // Returns the Web App manifest ID for the app, which is the canonical
  // identifier for this app, as specified by
  // https://www.w3.org/TR/appmanifest/#id-member. Does not attempt to validate
  // the value returned. Must only be called if `GetPlatform()` returns
  // `AppType::kWeb`.
  std::string GetWebAppManifestId() const;

  // Returns the Web App manifest URL for the app, which hosts the manifest of
  // the app in a JSON format. The URL could point to a local file, or a web
  // address. Does not attempt to validate the GURL. Must only be called if
  // `GetPlatform()` returns `AppType::kWeb`.
  GURL GetWebAppManifestUrl() const;

 private:
  proto::AppProvisioningListAppsResponse_App app_proto_;
};

std::ostream& operator<<(std::ostream& os, const PreloadAppDefinition& app);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_PRELOAD_APP_DEFINITION_H_

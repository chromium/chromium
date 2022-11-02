// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_PRELOAD_APP_DEFINITION_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_PRELOAD_APP_DEFINITION_H_

#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

// A wrapper class around an App Preload Server proto to allow for easier
// extraction and conversion of information.
class PreloadAppDefinition {
 public:
  explicit PreloadAppDefinition(proto::AppProvisioningResponse_App app_proto)
      : app_proto_(app_proto) {}
  PreloadAppDefinition(const PreloadAppDefinition&) = default;
  PreloadAppDefinition& operator=(const PreloadAppDefinition&) = default;
  ~PreloadAppDefinition() = default;

  std::string GetName() const;
  AppType GetPlatform() const;
  bool IsOemApp() const;

 private:
  proto::AppProvisioningResponse_App app_proto_;
};

std::ostream& operator<<(std::ostream& os, const PreloadAppDefinition& app);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_PRELOAD_APP_DEFINITION_H_

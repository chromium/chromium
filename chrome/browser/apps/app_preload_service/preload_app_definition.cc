// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"

namespace apps {

std::string PreloadAppDefinition::GetName() const {
  return app_proto_.name();
}

AppType PreloadAppDefinition::GetPlatform() const {
  switch (app_proto_.platform()) {
    case proto::AppProvisioningResponse_Platform::
        AppProvisioningResponse_Platform_PLATFORM_UNKNOWN:
      return AppType::kUnknown;
    case proto::AppProvisioningResponse_Platform::
        AppProvisioningResponse_Platform_PLATFORM_WEB:
      return AppType::kWeb;
    case proto::AppProvisioningResponse_Platform::
        AppProvisioningResponse_Platform_PLATFORM_ANDROID:
      return AppType::kArc;
  }
}

bool PreloadAppDefinition::IsOemApp() const {
  return app_proto_.install_reason() ==
         proto::AppProvisioningResponse_InstallReason::
             AppProvisioningResponse_InstallReason_INSTALL_REASON_OEM;
}

std::ostream& operator<<(std::ostream& os, const PreloadAppDefinition& app) {
  os << "- Name: " << app.GetName();
  os << "- Platform: " << EnumToString(app.GetPlatform());
  os << "- OEM: " << app.IsOemApp();
  return os;
}

}  // namespace apps

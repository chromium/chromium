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
    case proto::AppProvisioningResponse::PLATFORM_UNKNOWN:
      return AppType::kUnknown;
    case proto::AppProvisioningResponse::PLATFORM_WEB:
      return AppType::kWeb;
    case proto::AppProvisioningResponse::PLATFORM_ANDROID:
      return AppType::kArc;
  }
}

bool PreloadAppDefinition::IsOemApp() const {
  return app_proto_.install_reason() ==
         proto::AppProvisioningResponse_InstallReason::
             AppProvisioningResponse_InstallReason_INSTALL_REASON_OEM;
}

std::string PreloadAppDefinition::GetWebAppManifestId() const {
  DCHECK_EQ(GetPlatform(), AppType::kWeb);

  return app_proto_.web_extras().manifest_id();
}

std::ostream& operator<<(std::ostream& os, const PreloadAppDefinition& app) {
  os << std::boolalpha;
  os << "- Name: " << app.GetName() << std::endl;
  os << "- Platform: " << EnumToString(app.GetPlatform()) << std::endl;
  os << "- OEM: " << app.IsOemApp() << std::endl;

  if (app.GetPlatform() == AppType::kWeb) {
    os << "- Web Extras:" << std::endl;
    os << "  - Manifest ID: " << app.GetWebAppManifestId() << std::endl;
  }

  os << std::noboolalpha;
  return os;
}

}  // namespace apps

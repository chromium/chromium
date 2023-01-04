// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"

#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace apps {

std::string PreloadAppDefinition::GetName() const {
  return app_proto_.name();
}

// TODO(b/263437253): fix up once supporting libraries are in place.
AppType PreloadAppDefinition::GetPlatform() const {
  if (app_proto_.has_web_extras()) {
    return AppType::kWeb;
  }

  return AppType::kUnknown;
}

bool PreloadAppDefinition::IsOemApp() const {
  return app_proto_.install_reason() ==
         proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM;
}

GURL PreloadAppDefinition::GetWebAppManifestUrl() const {
  DCHECK_EQ(GetPlatform(), AppType::kWeb);

  return GURL(app_proto_.web_extras().manifest_url());
}

GURL PreloadAppDefinition::GetWebAppOriginalManifestUrl() const {
  DCHECK_EQ(GetPlatform(), AppType::kWeb);

  return GURL(app_proto_.web_extras().original_manifest_url());
}

GURL PreloadAppDefinition::GetWebAppManifestId() const {
  DCHECK_EQ(GetPlatform(), AppType::kWeb);

  // TODO(b/264199799): Replace this logic with package ID library methods.
  if (!base::StartsWith(app_proto_.package_id(), "web:")) {
    return GURL();
  }

  // The package_id of web apps are prepended with `web:`.
  std::string manifest_id = app_proto_.package_id().substr(strlen("web:"));

  return GURL(manifest_id);
}

std::ostream& operator<<(std::ostream& os, const PreloadAppDefinition& app) {
  os << std::boolalpha;
  os << "- Name: " << app.GetName() << std::endl;
  os << "- Platform: " << EnumToString(app.GetPlatform()) << std::endl;
  os << "- OEM: " << app.IsOemApp() << std::endl;

  if (app.GetPlatform() == AppType::kWeb) {
    os << "- Web Extras:" << std::endl;
    os << "  - Manifest URL: " << app.GetWebAppManifestUrl() << std::endl;
    os << "  - Original Manifest URL: " << app.GetWebAppOriginalManifestUrl()
       << std::endl;
    os << "  - Manifest ID: " << app.GetWebAppManifestId() << std::endl;
  }

  os << std::noboolalpha;
  return os;
}

}  // namespace apps

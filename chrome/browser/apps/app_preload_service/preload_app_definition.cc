// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"

#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_preload_service/proto/app_preload.pb.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "url/gurl.h"

namespace apps {

PreloadAppDefinition::PreloadAppDefinition(
    proto::AppPreloadListResponse_App app_proto)
    : app_proto_(app_proto),
      package_id_(PackageId::FromString(app_proto_.package_id())) {}

PreloadAppDefinition::PreloadAppDefinition(const PreloadAppDefinition&) =
    default;
PreloadAppDefinition& PreloadAppDefinition::operator=(
    const PreloadAppDefinition&) = default;
PreloadAppDefinition::~PreloadAppDefinition() = default;

std::string PreloadAppDefinition::GetName() const {
  return app_proto_.name();
}

AppType PreloadAppDefinition::GetPlatform() const {
  if (package_id_.has_value()) {
    return package_id_->app_type();
  }
  return AppType::kUnknown;
}

bool PreloadAppDefinition::IsOemApp() const {
  return app_proto_.install_reason() ==
         proto::AppPreloadListResponse::INSTALL_REASON_OEM;
}

bool PreloadAppDefinition::IsTestApp() const {
  return app_proto_.install_reason() ==
         proto::AppPreloadListResponse::INSTALL_REASON_TEST;
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
  DCHECK(package_id_.has_value());

  return GURL(package_id_->identifier());
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

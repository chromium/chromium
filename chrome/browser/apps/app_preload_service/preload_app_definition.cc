// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_preload_service/proto/app_preload.pb.h"
#include "components/services/app_service/public/cpp/package_id.h"
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

std::optional<PackageId> PreloadAppDefinition::GetPackageId() const {
  return package_id_;
}

std::string PreloadAppDefinition::GetName() const {
  return app_proto_.name();
}

PackageType PreloadAppDefinition::GetPlatform() const {
  if (package_id_.has_value()) {
    return package_id_->package_type();
  }
  return PackageType::kUnknown;
}

bool PreloadAppDefinition::IsDefaultApp() const {
  return app_proto_.install_reason() ==
         proto::AppPreloadListResponse::INSTALL_REASON_DEFAULT;
}

bool PreloadAppDefinition::IsOemApp() const {
  return app_proto_.install_reason() ==
         proto::AppPreloadListResponse::INSTALL_REASON_OEM;
}

bool PreloadAppDefinition::IsTestApp() const {
  return app_proto_.install_reason() ==
         proto::AppPreloadListResponse::INSTALL_REASON_TEST;
}

AppInstallSurface PreloadAppDefinition::GetInstallSurface() const {
  return IsDefaultApp() ? AppInstallSurface::kAppPreloadServiceDefault
                        : AppInstallSurface::kAppPreloadServiceOem;
}

std::string PreloadAppDefinition::GetAndroidPackageName() const {
  DCHECK_EQ(GetPlatform(), PackageType::kArc);
  DCHECK(package_id_.has_value());

  return package_id_->identifier();
}

GURL PreloadAppDefinition::GetWebAppManifestUrl() const {
  DCHECK_EQ(GetPlatform(), PackageType::kWeb);

  return GURL(app_proto_.web_extras().manifest_url());
}

GURL PreloadAppDefinition::GetWebAppOriginalManifestUrl() const {
  DCHECK_EQ(GetPlatform(), PackageType::kWeb);

  return GURL(app_proto_.web_extras().original_manifest_url());
}

GURL PreloadAppDefinition::GetWebAppManifestId() const {
  DCHECK_EQ(GetPlatform(), PackageType::kWeb);
  DCHECK(package_id_.has_value());

  return GURL(package_id_->identifier());
}

AppInstallData PreloadAppDefinition::ToAppInstallData() const {
  AppInstallData result(package_id_.value());
  result.name = GetName();
  if (GetPlatform() == PackageType::kArc) {
    // nothing.
  } else if (GetPlatform() == PackageType::kWeb) {
    auto& web_app_data = result.app_type_data.emplace<WebAppInstallData>();
    web_app_data.original_manifest_url = GetWebAppOriginalManifestUrl();
    web_app_data.proxied_manifest_url = GetWebAppManifestUrl();
    web_app_data.document_url = GetWebAppManifestId().GetWithEmptyPath();
  } else {
    NOTREACHED_IN_MIGRATION();
  }
  return result;
}

std::ostream& operator<<(std::ostream& os, const PreloadAppDefinition& app) {
  os << std::boolalpha;
  os << "- Package ID: "
     << (app.GetPackageId() ? app.GetPackageId()->ToString() : std::string())
     << std::endl;
  os << "- Name: " << app.GetName() << std::endl;
  os << "- Platform: " << EnumToString(app.GetPlatform()) << std::endl;
  os << "- OEM: " << app.IsOemApp() << std::endl;
  os << "- Default: " << app.IsDefaultApp() << std::endl;

  if (app.GetPlatform() == PackageType::kArc) {
    os << "- Android Extras:" << std::endl;
    os << "  - Package Name: " << app.GetAndroidPackageName() << std::endl;
  } else if (app.GetPlatform() == PackageType::kWeb) {
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

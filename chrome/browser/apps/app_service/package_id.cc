// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/package_id.h"

#include <string>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

namespace {
constexpr char kArcPlatformName[] = "android";
constexpr char kWebPlatformName[] = "web";

AppType PlatformNameToAppType(base::StringPiece platform_name) {
  if (platform_name == kArcPlatformName) {
    return AppType::kArc;
  }
  if (platform_name == kWebPlatformName) {
    return AppType::kWeb;
  }

  return AppType::kUnknown;
}

base::StringPiece AppTypeToPlatformName(AppType app_type) {
  switch (app_type) {
    case AppType::kArc:
      return kArcPlatformName;
    case AppType::kWeb:
      return kWebPlatformName;
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

PackageId::PackageId(AppType app_type, base::StringPiece identifier)
    : app_type_(app_type), identifier_(identifier) {
  DCHECK(app_type_ == AppType::kArc || app_type_ == AppType::kWeb);
  DCHECK(!identifier_.empty());
}

PackageId::PackageId(const PackageId&) = default;
PackageId& PackageId::operator=(const PackageId&) = default;

bool PackageId::operator<(const PackageId& rhs) const {
  if (this->app_type_ < rhs.app_type_) {
    return true;
  } else if (this->app_type_ > rhs.app_type_) {
    return false;
  }
  // If we're here, it's because app_type_ == rhs.app_type_.
  if (this->identifier_ < rhs.identifier_) {
    return true;
  } else {
    return false;
  }
}

bool PackageId::operator==(const PackageId& rhs) const {
  return this->app_type_ == rhs.app_type_ &&
         this->identifier_ == rhs.identifier_;
}

// static
absl::optional<PackageId> PackageId::FromString(
    base::StringPiece package_id_string) {
  size_t separator = package_id_string.find_first_of(':');
  if (separator == std::string::npos ||
      separator == package_id_string.size() - 1) {
    return absl::nullopt;
  }

  AppType type = PlatformNameToAppType(package_id_string.substr(0, separator));
  if (type == AppType::kUnknown) {
    return absl::nullopt;
  }

  return PackageId(type, package_id_string.substr(separator + 1));
}

std::string PackageId::ToString() const {
  return base::StrCat({AppTypeToPlatformName(app_type_), ":", identifier_});
}

}  // namespace apps

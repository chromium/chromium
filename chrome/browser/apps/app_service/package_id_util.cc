// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/package_id_util.h"

#include "chrome/browser/apps/app_service/package_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps_util {

absl::optional<apps::PackageId> GetPackageIdForApp(
    const apps::AppUpdate& update) {
  if (update.AppType() != apps::AppType::kArc &&
      update.AppType() != apps::AppType::kWeb) {
    return absl::nullopt;
  }
  if (update.PublisherId().empty()) {
    return absl::nullopt;
  }
  // TODO(b/297309305): Update this to create Android-type package IDs for TWAs
  // that come through from ARC.
  return apps::PackageId(update.AppType(), update.PublisherId());
}

}  // namespace apps_util

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_mapper.h"

#include "base/logging.h"

namespace apps::deduplication {

AppDeduplicationMapper::AppDeduplicationMapper() = default;

AppDeduplicationMapper::~AppDeduplicationMapper() = default;

absl::optional<proto::DeduplicateData>
AppDeduplicationMapper::ToDeduplicateData(
    const proto::DeduplicateResponse& response) {
  if (response.app_group().empty()) {
    LOG(ERROR) << "No duplicate groups found.";
    return absl::nullopt;
  }

  proto::DeduplicateData deduplicate_data;
  for (const auto& group : response.app_group()) {
    if (group.app().empty()) {
      LOG(ERROR) << "No apps found in duplicate group.";
      return absl::nullopt;
    }

    auto* deduplicate_group = deduplicate_data.add_app_group();
    for (const auto& app : group.app()) {
      if (!app.has_platform() || app.platform().empty()) {
        LOG(ERROR) << "The platform for an app cannot be empty.";
        return absl::nullopt;
      }

      if (!app.has_app_id() || app.app_id().empty()) {
        LOG(ERROR) << "The app_id for an app cannot be empty.";
        return absl::nullopt;
      }

      auto* deduplicate_app = deduplicate_group->add_app();
      deduplicate_app->set_app_id(app.app_id());
      deduplicate_app->set_platform(app.platform());
    }
  }

  return deduplicate_data;
}

}  // namespace apps::deduplication

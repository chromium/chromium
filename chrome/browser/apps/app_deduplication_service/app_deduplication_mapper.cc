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
    auto* deduplicate_group = deduplicate_data.add_app_group();
    if (group.app_group_uuid().empty()) {
      LOG(ERROR) << "The uuid for an app group cannot be empty.";
      return absl::nullopt;
    }
    deduplicate_group->set_app_group_uuid(group.app_group_uuid());

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
      deduplicate_app->set_platform(app.platform());
      deduplicate_app->set_app_id(app.app_id());
    }

    if (group.package_id().empty()) {
      LOG(ERROR) << "An app group must have at least 1 package id.";
      return absl::nullopt;
    }

    for (int i = 0; i < group.package_id_size(); i++) {
      deduplicate_group->add_package_id();
      deduplicate_group->set_package_id(i, group.package_id(i));
    }
  }

  return deduplicate_data;
}

}  // namespace apps::deduplication

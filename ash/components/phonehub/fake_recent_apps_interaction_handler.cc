// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/fake_recent_apps_interaction_handler.h"
#include "ash/components/phonehub/notification.h"
#include "base/containers/contains.h"

namespace ash {
namespace phonehub {

using FeatureState = multidevice_setup::mojom::FeatureState;

FakeRecentAppsInteractionHandler::FakeRecentAppsInteractionHandler() = default;

FakeRecentAppsInteractionHandler::~FakeRecentAppsInteractionHandler() = default;

void FakeRecentAppsInteractionHandler::NotifyRecentAppClicked(
    const Notification::AppMetadata& app_metadata) {
  if (base::Contains(package_name_to_click_count_, app_metadata.package_name)) {
    package_name_to_click_count_.at(app_metadata.package_name)++;
    return;
  }
  package_name_to_click_count_[app_metadata.package_name] = 1;
}

void FakeRecentAppsInteractionHandler::AddRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  recent_app_click_observer_count_++;
}

void FakeRecentAppsInteractionHandler::RemoveRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  recent_app_click_observer_count_--;
}

void FakeRecentAppsInteractionHandler::OnFeatureStateChanged(
    FeatureState feature_state) {
  feature_state_ = feature_state;
  ComputeAndUpdateUiState();
}

void FakeRecentAppsInteractionHandler::NotifyRecentAppAddedOrUpdated(
    const Notification::AppMetadata& app_metadata,
    base::Time last_accessed_timestamp) {
  recent_apps_metadata_.emplace_back(app_metadata, last_accessed_timestamp);
}

std::vector<Notification::AppMetadata>
FakeRecentAppsInteractionHandler::FetchRecentAppMetadataList() {
  std::vector<Notification::AppMetadata> app_metadata_list;
  for (const auto& recent_app_metadata : recent_apps_metadata_) {
    app_metadata_list.emplace_back(recent_app_metadata.first);
  }
  return app_metadata_list;
}

void FakeRecentAppsInteractionHandler::ComputeAndUpdateUiState() {
  if (feature_state_ != FeatureState::kEnabledByUser) {
    ui_state_ = RecentAppsUiState::HIDDEN;
    return;
  }
  ui_state_ = recent_apps_metadata_.empty()
                  ? RecentAppsUiState::PLACEHOLDER_VIEW
                  : RecentAppsUiState::ITEMS_VISIBLE;
}

}  // namespace phonehub
}  // namespace ash

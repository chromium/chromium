// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_provider.h"

#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_item.h"
#include "ash/shell.h"

namespace {
bool HasValidClusterCount(size_t num_clusters) {
  return num_clusters <= 2;
}
}  // namespace

namespace ash {

BirchCoralProvider::BirchCoralProvider(BirchModel* birch_model)
    : birch_model_(birch_model) {
  if (features::IsTabClusterUIEnabled()) {
    Shell::Get()->tab_cluster_ui_controller()->AddObserver(this);
  }
}

BirchCoralProvider::~BirchCoralProvider() {
  if (features::IsTabClusterUIEnabled()) {
    Shell::Get()->tab_cluster_ui_controller()->RemoveObserver(this);
  }
}

void BirchCoralProvider::OnTabItemAdded(TabClusterUIItem* tab_item) {
  // TODO(yulunwu) stream tab item metadata to backend for async embedding
}

void BirchCoralProvider::OnTabItemUpdated(TabClusterUIItem* tab_item) {
  // TODO(yulunwu) stream tab item metadata to backend for async embedding
}

void BirchCoralProvider::OnTabItemRemoved(TabClusterUIItem* tab_item) {}

void BirchCoralProvider::RequestBirchDataFetch() {
  // TODO(yulunwu) make appropriate data request, send data to backend.
  if (HasValidPostLoginData()) {
    HandlePostLoginDataRequest();
  } else {
    HandleInSessionDataRequest();
  }
}

bool BirchCoralProvider::HasValidPostLoginData() const {
  // TODO(sammiequon) add check for valid post login data.
  return false;
}

void BirchCoralProvider::HandlePostLoginDataRequest() {
  // TODO(sammiequon) handle post-login use case.
}

void BirchCoralProvider::HandleInSessionDataRequest() {
  // TODO(yulunwu, zxdan) add more tab metadata, app data,
  // and handle in-session use cases.
  std::vector<coral_util::TabData> active_tab_data;
  for (const std::unique_ptr<TabClusterUIItem>& tab :
       Shell::Get()->tab_cluster_ui_controller()->tab_items()) {
    active_tab_data.emplace_back(
        coral_util::TabData{.tab_title = tab->current_info().title,
                            .source = tab->current_info().source});
  }
  request_.set_tab_data(std::move(active_tab_data));
}

void BirchCoralProvider::HandleCoralResponse(
    std::unique_ptr<coral_util::CoralResponse> response) {
  // TODO(yulunwu) update `birch_model_`
  response_ = std::move(response);
  CHECK(HasValidClusterCount(response_->clusters().size()));
}

}  // namespace ash

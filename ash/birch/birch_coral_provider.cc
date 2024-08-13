// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_provider.h"

#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"
#include "ash/shell.h"

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
}

void HandlePostLoginDataRequest() {
  // TODO(sammiequon) handle post-login use case.
}

void HandleInSessionDataRequest() {
  // TODO(yulunwu, zxdan) handle in-session use case.
}

void BirchCoralProvider::HandleClusterData() {
  // TODO(yulunwu) update `birch_model_`
}

}  // namespace ash

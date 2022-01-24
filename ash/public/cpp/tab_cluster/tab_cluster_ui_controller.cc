// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"

#include "ash/public/cpp/tab_cluster/tab_cluster_ui_item.h"
#include "base/containers/unique_ptr_adapters.h"

namespace ash {

TabClusterUIController::TabClusterUIController() = default;

TabClusterUIController::~TabClusterUIController() = default;

TabClusterUIItem* TabClusterUIController::AddTabItem(
    std::unique_ptr<TabClusterUIItem> tab_item) {
  auto* tab_item_raw = tab_item.get();
  tab_items_.push_back(std::move(tab_item));
  for (auto& observer : observers_)
    observer.OnTabItemAdded(tab_item_raw);
  return tab_item_raw;
}

void TabClusterUIController::UpdateTabItem(TabClusterUIItem* tab_item) {
  DCHECK(std::find_if(tab_items_.begin(), tab_items_.end(),
                      base::MatchesUniquePtr(tab_item)) != tab_items_.end());
  for (auto& observer : observers_)
    observer.OnTabItemUpdated(tab_item);
}

void TabClusterUIController::RemoveTabItem(TabClusterUIItem* tab_item) {
  auto end_iter = tab_items_.end();
  auto iter = std::find_if(tab_items_.begin(), end_iter,
                           base::MatchesUniquePtr(tab_item));
  DCHECK(iter != end_iter);
  // Since observer may need to use item values, notify observer before removing
  // from item list.
  for (auto& observer : observers_)
    observer.OnTabItemRemoved(tab_item);
  tab_items_.erase(iter);
}

void TabClusterUIController::ChangeActiveCandidate(
    TabClusterUIItem* old_active_item,
    TabClusterUIItem* new_active_item) {
  for (auto* const tab_item : clusterer_.GetUpdatedClusterInfo(
           tab_items_, old_active_item, new_active_item)) {
    for (auto& observer : observers_)
      observer.OnTabItemUpdated(tab_item);
  }
}

void TabClusterUIController::AddObserver(
    TabClusterUIController::Observer* observer) {
  observers_.AddObserver(observer);
}

void TabClusterUIController::RemoveObserver(
    TabClusterUIController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash

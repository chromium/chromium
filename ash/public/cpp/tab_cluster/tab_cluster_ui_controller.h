// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TAB_CLUSTER_TAB_CLUSTER_UI_CONTROLLER_H_
#define ASH_PUBLIC_CPP_TAB_CLUSTER_TAB_CLUSTER_UI_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/tab_cluster/clusterer.h"
#include "base/observer_list.h"

namespace ash {
class TabClusterUIItem;

// TabClusterUIController:
// Manage the tab items of the opened, modified and closed tabs. When there is
// a tab item changed, it will notify its observers.
class ASH_PUBLIC_EXPORT TabClusterUIController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnTabItemAdded(TabClusterUIItem* tab_item) = 0;
    virtual void OnTabItemUpdated(TabClusterUIItem* tab_item) = 0;
    virtual void OnTabItemRemoved(TabClusterUIItem* tab_item) = 0;
  };

  using TabItems = std::vector<std::unique_ptr<TabClusterUIItem>>;

  TabClusterUIController();
  TabClusterUIController(const TabClusterUIController&) = delete;
  TabClusterUIController& operator=(const TabClusterUIController&) = delete;

  ~TabClusterUIController();

  TabClusterUIItem* AddTabItem(std::unique_ptr<TabClusterUIItem> tab_item);
  void UpdateTabItem(TabClusterUIItem* tab_item);
  void RemoveTabItem(TabClusterUIItem* tab_item);
  void ChangeActiveCandidate(TabClusterUIItem* old_active_item,
                             TabClusterUIItem* new_active_item);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const TabItems& tab_items() const { return tab_items_; }

 private:
  Clusterer clusterer_;
  // List of tab items.
  TabItems tab_items_;
  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TAB_CLUSTER_TAB_CLUSTER_UI_CONTROLLER_H_

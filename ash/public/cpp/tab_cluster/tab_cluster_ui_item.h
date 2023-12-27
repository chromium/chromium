// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TAB_CLUSTER_TAB_CLUSTER_UI_ITEM_H_
#define ASH_PUBLIC_CPP_TAB_CLUSTER_TAB_CLUSTER_UI_ITEM_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/memory/raw_ptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// TabClusterUIItem includes realtime info of each tab opened in the browser.
class ASH_PUBLIC_EXPORT TabClusterUIItem {
 public:
  struct Info {
    Info();

    Info(const Info&);
    Info& operator=(const Info&);

    ~Info();

    // The tab title.
    std::string title;
    // The url or source link of a tab.
    std::string source;
    // The cluster to which the tab belongs.
    int cluster_id = -1;
    // The boundary strength of the cluster.
    double boundary_strength = 0.0;
    // The browser window that holds the tab's contents.
    raw_ptr<aura::Window> browser_window = nullptr;
  };

  TabClusterUIItem();
  explicit TabClusterUIItem(const Info& info);
  TabClusterUIItem(const TabClusterUIItem&) = delete;
  TabClusterUIItem& operator=(const TabClusterUIItem&) = delete;

  ~TabClusterUIItem();

  // Load in info.
  void Init(const Info& info);

  Info current_info() const { return current_info_; }
  Info old_info() const { return old_info_; }
  void SetCurrentClusterId(int cluster_id);
  void SetCurrentBoundaryStrength(double boundary_strength);

 private:
  // Current tab item info.
  Info current_info_;
  // The last replaced tab item info.
  Info old_info_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TAB_CLUSTER_TAB_CLUSTER_UI_ITEM_H_

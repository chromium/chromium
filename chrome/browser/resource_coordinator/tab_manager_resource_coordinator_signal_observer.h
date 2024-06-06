// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {

// TabManager::ResourceCoordinatorSignalObserver forwards data from the
// performance manager graph. All functions run on the PerformanceManager
// sequence unless otherwise noted.
// TODO(chrisha): Kill this thing entirely and move all of tab manager into the
// performance manager.
class TabManagerResourceCoordinatorSignalObserver
    : public performance_manager::GraphOwned,
      public performance_manager::PageNode::ObserverDefaultImpl {
 public:
  using Graph = performance_manager::Graph;
  using PageNode = performance_manager::PageNode;

  TabManagerResourceCoordinatorSignalObserver() = default;
  ~TabManagerResourceCoordinatorSignalObserver() override = default;

  TabManagerResourceCoordinatorSignalObserver(
      const TabManagerResourceCoordinatorSignalObserver&) = delete;
  TabManagerResourceCoordinatorSignalObserver& operator=(
      const TabManagerResourceCoordinatorSignalObserver&) = delete;

  // PageNode::ObserverDefaultImpl:
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 private:
  // Posted to the UI thread from the GraphObserver functions above.
  static void OnPageStoppedLoadingOnUi(
      base::WeakPtr<content::WebContents> contents);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_

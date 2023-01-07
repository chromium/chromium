// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_

#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"

namespace resource_coordinator {

// TabManager::ResourceCoordinatorSignalObserver forwards data from the
// performance manager graph.
// TODO(chrisha): Kill this thing entirely and move all of tab manager into the
// performance manager.
class TabManager::ResourceCoordinatorSignalObserver
    : public performance_manager::GraphOwned,
      public performance_manager::ProcessNode::ObserverDefaultImpl,
      public performance_manager::PageNode::ObserverDefaultImpl {
 public:
  using Graph = performance_manager::Graph;
  using PageNode = performance_manager::PageNode;
  using ProcessNode = performance_manager::ProcessNode;
  using WebContentsProxy = performance_manager::WebContentsProxy;

  explicit ResourceCoordinatorSignalObserver(
      const base::WeakPtr<TabManager>& tab_manager);

  ResourceCoordinatorSignalObserver(const ResourceCoordinatorSignalObserver&) =
      delete;
  ResourceCoordinatorSignalObserver& operator=(
      const ResourceCoordinatorSignalObserver&) = delete;

  ~ResourceCoordinatorSignalObserver() override;

  // PageNode::ObserverDefaultImpl:
  // This function run on the performance manager sequence.
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 private:
  // Determines if a message should still be dispatched for the given
  // |tab_manager|, |contents_proxy| and |navigation_id|. If so, returns the
  // WebContents, otherwise returns nullptr. This should only be called on the
  // UI thread.
  static content::WebContents* GetContentsForDispatch(
      const base::WeakPtr<TabManager>& tab_manager,
      const WebContentsProxy& contents_proxy,
      int64_t navigation_id);

  // Posted to the UI thread from the GraphObserver functions above.
  static void OnPageStoppedLoadingOnUi(const WebContentsProxy& contents_proxy);

  // Can only be dereferenced on the UI thread. When the tab manager dies this
  // is used to drop messages received from the performance manager. Ideally
  // we'd also then tear down this observer on the perf manager sequence itself,
  // but when one dies they're both about to die.
  base::WeakPtr<TabManager> tab_manager_;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_

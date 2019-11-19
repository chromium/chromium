// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_

#include "base/macros.h"
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
  ~ResourceCoordinatorSignalObserver() override;

  // ProcessNode::ObserverDefaultImpl:
  // This function run on the performance manager sequence.
  void OnExpectedTaskQueueingDurationSample(
      const ProcessNode* process_node) override;

  // PageNode::ObserverDefaultImpl:
  // This function run on the performance manager sequence.
  void OnPageAlmostIdleChanged(const PageNode* page_node) override;

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

  // Equivalent to the the GraphObserver functions above, but these are the
  // counterparts that run on the UI thread.
  static void OnPageAlmostIdleOnUi(const base::WeakPtr<TabManager>& tab_manager,
                                   const WebContentsProxy& contents_proxy,
                                   int64_t navigation_id);
  static void OnExpectedTaskQueueingDurationSampleOnUi(
      const base::WeakPtr<TabManager>& tab_manager,
      const WebContentsProxy& contents_proxy,
      int64_t navigation_id,
      base::TimeDelta duration);

  // Can only be dereferenced on the UI thread. When the tab manager dies this
  // is used to drop messages received from the performance manager. Ideally
  // we'd also then tear down this observer on the perf manager sequence itself,
  // but when one dies they're both about to die.
  base::WeakPtr<TabManager> tab_manager_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorSignalObserver);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_

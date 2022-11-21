// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_resource_coordinator_signal_observer.h"

#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace resource_coordinator {

// A helper class for accessing TabLoadTracker. TabLoadTracker can't directly
// friend TabManager::ResourceCoordinatorSignalObserver as it's a nested class
// and can't be forward declared.
class TabManagerResourceCoordinatorSignalObserverHelper {
 public:
  static void OnPageStoppedLoading(content::WebContents* web_contents) {
    TabLoadTracker::Get()->OnPageStoppedLoading(web_contents);
  }
};

TabManager::ResourceCoordinatorSignalObserver::
    ResourceCoordinatorSignalObserver(
        const base::WeakPtr<TabManager>& tab_manager)
    : tab_manager_(tab_manager) {}

TabManager::ResourceCoordinatorSignalObserver::
    ~ResourceCoordinatorSignalObserver() = default;

void TabManager::ResourceCoordinatorSignalObserver::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  // Forward the notification over to the UI thread when the page stops loading.
  if (page_node->GetLoadingState() == PageNode::LoadingState::kLoadedIdle) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&OnPageStoppedLoadingOnUi,
                                  page_node->GetContentsProxy()));
  }
}

void TabManager::ResourceCoordinatorSignalObserver::OnPassedToGraph(
    Graph* graph) {
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
}

void TabManager::ResourceCoordinatorSignalObserver::OnTakenFromGraph(
    Graph* graph) {
  graph->RemovePageNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);
}

// static
content::WebContents*
TabManager::ResourceCoordinatorSignalObserver::GetContentsForDispatch(
    const base::WeakPtr<TabManager>& tab_manager,
    const WebContentsProxy& contents_proxy,
    int64_t navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!tab_manager.get() || !contents_proxy.Get() ||
      contents_proxy.LastNavigationId() != navigation_id) {
    return nullptr;
  }
  return contents_proxy.Get();
}

// static
void TabManager::ResourceCoordinatorSignalObserver::OnPageStoppedLoadingOnUi(
    const WebContentsProxy& contents_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* contents = contents_proxy.Get()) {
    TabManagerResourceCoordinatorSignalObserverHelper::OnPageStoppedLoading(
        contents);
  }
}

}  // namespace resource_coordinator

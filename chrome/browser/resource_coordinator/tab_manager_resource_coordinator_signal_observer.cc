// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_resource_coordinator_signal_observer.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace resource_coordinator {

void TabManagerResourceCoordinatorSignalObserver::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  // Forward the notification over to the UI thread when the page stops loading.
  if (page_node->GetLoadingState() == PageNode::LoadingState::kLoadedIdle) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&OnPageStoppedLoadingOnUi, page_node->GetWebContents()));
  }
}

void TabManagerResourceCoordinatorSignalObserver::OnPassedToGraph(
    Graph* graph) {
  graph->AddPageNodeObserver(this);
}

void TabManagerResourceCoordinatorSignalObserver::OnTakenFromGraph(
    Graph* graph) {
  graph->RemovePageNodeObserver(this);
}

// static
void TabManagerResourceCoordinatorSignalObserver::OnPageStoppedLoadingOnUi(
    base::WeakPtr<content::WebContents> contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (contents) {
    TabLoadTracker::Get()->OnPageStoppedLoading(contents.get());
  }
}

}  // namespace resource_coordinator

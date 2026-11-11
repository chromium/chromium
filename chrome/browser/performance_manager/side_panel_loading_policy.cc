// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/side_panel_loading_policy.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/performance_manager/priority/side_panel_loading_voter.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager::priority {

void MarkAsSidePanel(content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents);
  CHECK(page_node);

  Graph* graph = PerformanceManager::GetGraph();
  auto* voter = graph->GetRegisteredObjectAs<priority::SidePanelLoadingVoter>();
  // Not registered in unit tests that don't initialize browser PM voters.
  if (!voter) {
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kPMSidePanelLoadingVoter)) {
    return;
  }

  voter->MarkAsSidePanel(page_node.get());
}

}  // namespace performance_manager::priority

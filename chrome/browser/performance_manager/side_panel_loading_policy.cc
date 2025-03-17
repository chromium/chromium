// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/side_panel_loading_policy.h"

#include "base/functional/bind.h"
#include "chrome/browser/performance_manager/execution_context_priority/side_panel_loading_voter.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager::execution_context_priority {

void MarkAsSidePanel(content::WebContents* web_contents) {
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents);
  CHECK(page_node);

  Graph* graph = PerformanceManager::GetGraph();
  auto* voter = graph->GetRegisteredObjectAs<
      execution_context_priority::SidePanelLoadingVoter>();
  CHECK(voter);

  voter->MarkAsSidePanel(page_node.get());
}

}  // namespace performance_manager::execution_context_priority

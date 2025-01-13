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
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node, Graph* graph) {
            CHECK(page_node);
            auto* voter = graph->GetRegisteredObjectAs<
                execution_context_priority::SidePanelLoadingVoter>();
            CHECK(voter);

            voter->MarkAsSidePanel(page_node.get());
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents)));
}

}  // namespace performance_manager::execution_context_priority

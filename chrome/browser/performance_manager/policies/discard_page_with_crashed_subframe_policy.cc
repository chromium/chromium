// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/discard_page_with_crashed_subframe_policy.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

namespace performance_manager::policies {

DiscardPageWithCrashedSubframePolicy::DiscardPageWithCrashedSubframePolicy() =
    default;

DiscardPageWithCrashedSubframePolicy::~DiscardPageWithCrashedSubframePolicy() =
    default;

void DiscardPageWithCrashedSubframePolicy::
    OnCrossProcessSubframeRenderProcessGone(const FrameNode* frame_node) {
  DiscardEligibilityPolicy* eligiblity_policy =
      DiscardEligibilityPolicy::GetFromGraph(GetOwningGraph());
  DCHECK(eligiblity_policy);
  // Whether the page will be reloaded after the subframe crash by
  // ReloadHiddenTabsWithCrashedSubframes feature.
  bool will_reload = !frame_node->GetPageNode()->IsVisible() &&
                     frame_node->IsRendered() && frame_node->IsActive();
  bool is_eligible =
      will_reload && eligiblity_policy->CanDiscard(
                         frame_node->GetPageNode(),
                         DiscardEligibilityPolicy::DiscardReason::URGENT,
                         base::TimeDelta()) == CanDiscardResult::kEligible;
  base::UmaHistogramBoolean("Stability.ChildFrameCrash.PageEligibleToDiscard",
                            is_eligible);
  if (!is_eligible) {
    return;
  }
  content::WebContents* web_contents =
      frame_node->GetPageNode()->GetWebContents().get();
  if (web_contents &&
      base::FeatureList::IsEnabled(features::kWebContentsDiscard)) {
    web_contents->Discard(base::NullCallback());
  }
}

void DiscardPageWithCrashedSubframePolicy::OnPassedToGraph(Graph* graph) {
  graph->AddFrameNodeObserver(this);
}

void DiscardPageWithCrashedSubframePolicy::OnTakenFromGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
}

}  // namespace performance_manager::policies

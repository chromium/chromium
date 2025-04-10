// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/process_rank_policy_android.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/not_fatal_until.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"

namespace performance_manager::policies {

namespace {

content::ChildProcessImportance CalculateRank(const PageNode* page_node) {
  if (page_node->IsFocused()) {
    return content::ChildProcessImportance::IMPORTANT;
  } else if (page_node->IsVisible()) {
    if (base::FeatureList::IsEnabled(
            chrome::android::kChangeUnfocusedPriority)) {
      return content::ChildProcessImportance::MODERATE;
    } else {
      return content::ChildProcessImportance::IMPORTANT;
    }
  }
  const PageLiveStateDecorator::Data* live_state_data =
      PageLiveStateDecorator::Data::FromPageNode(page_node);
  if (live_state_data && live_state_data->IsActiveTab()) {
    return content::ChildProcessImportance::MODERATE;
  }

  // TODO(crbug.com/399024097): check protected tab.

  return content::ChildProcessImportance::NORMAL;
}

}  // namespace

ProcessRankPolicyAndroid::ProcessRankPolicyAndroid() = default;
ProcessRankPolicyAndroid::~ProcessRankPolicyAndroid() = default;

void ProcessRankPolicyAndroid::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph->AddPageNodeObserver(this);
}

void ProcessRankPolicyAndroid::OnTakenFromGraph(Graph* graph) {
  graph->RemovePageNodeObserver(this);
}

void ProcessRankPolicyAndroid::OnPageNodeAdded(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)->AddObserver(
      this);
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->RemoveObserver(this);
}

void ProcessRankPolicyAndroid::OnIsFocusedChanged(const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsVisibleChanged(const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsActiveTabChanged(const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::UpdateProcessRank(const PageNode* page_node) {
  content::ChildProcessImportance importance = CalculateRank(page_node);

  const base::WeakPtr<content::WebContents> web_contents =
      page_node->GetWebContents();
  CHECK(web_contents, base::NotFatalUntil::M140);
  if (web_contents) {
    web_contents->SetPrimaryMainFrameImportance(importance);
  }
}

}  // namespace performance_manager::policies

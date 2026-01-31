// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/process_rank_policy_android.h"

#include <utility>

#include "base/android/android_info.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/not_fatal_until.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "content/public/browser/android/child_process_importance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

namespace performance_manager::policies {

ProcessRankPolicyAndroid::ProcessRankPolicyAndroid()
    : ProcessRankPolicyAndroid(content::IsPerceptibleImportanceSupported()) {}

ProcessRankPolicyAndroid::ProcessRankPolicyAndroid(
    bool is_perceptible_importance_supported)
    : is_perceptible_importance_supported_(
          is_perceptible_importance_supported) {}

ProcessRankPolicyAndroid::~ProcessRankPolicyAndroid() = default;

void ProcessRankPolicyAndroid::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph->AddPageNodeObserver(this);
}

void ProcessRankPolicyAndroid::OnTakenFromGraph(Graph* graph) {
  graph->RemovePageNodeObserver(this);
  if (base::FeatureList::IsEnabled(
          chrome::android::kProtectRecentlyVisibleTab)) {
    visibility_timers_.clear();
  }
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
  if (base::FeatureList::IsEnabled(
          chrome::android::kProtectRecentlyVisibleTab)) {
    visibility_timers_.erase(page_node);
  }
}

void ProcessRankPolicyAndroid::OnTypeChanged(const PageNode* page_node,
                                             PageType previous_type) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsFocusedChanged(const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsVisibleChanged(const PageNode* page_node) {
  if (base::FeatureList::IsEnabled(
          chrome::android::kProtectRecentlyVisibleTab)) {
    if (page_node->IsVisible()) {
      visibility_timers_.erase(page_node);
    } else {
      // If the page becomes invisible, start a timer to update the rank after
      // `minimum_time_in_background`.
      base::TimeDelta minimum_time_in_background = base::Seconds(
          chrome::android::kProtectRecentlyVisibleTabDuration.Get());

      // If `minimum_time_in_background` is zero, we can skip the timer since
      // the rank calculated via the call to `UpdateProcessRank` won't need to
      // be downgraded.
      if (!minimum_time_in_background.is_zero()) {
        auto& timer = visibility_timers_[page_node];
        if (!timer) {
          timer = std::make_unique<base::OneShotTimer>();
        }
        timer->Start(
            FROM_HERE, minimum_time_in_background,
            base::BindOnce(
                &ProcessRankPolicyAndroid::UpdateProcessRankAndClearTimer,
                base::Unretained(this), page_node));
      }
    }
  }
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsAudibleChanged(const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnHasPictureInPictureChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnMainFrameUrlChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnMainFrameDocumentChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnPageNotificationPermissionStatusChange(
    const PageNode* page_node,
    std::optional<blink::mojom::PermissionStatus> previous_status) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnHadFormInteractionChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}
void ProcessRankPolicyAndroid::OnHadUserEditsChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsActiveTabChanged(const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsAutoDiscardableChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsCapturingVideoChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsCapturingAudioChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsBeingMirroredChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsCapturingWindowChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsCapturingDisplayChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsConnectedToBluetoothDeviceChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsConnectedToUSBDeviceChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsPinnedTabChanged(const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnIsDevToolsOpenChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::OnUpdatedTitleOrFaviconInBackgroundChanged(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
}

void ProcessRankPolicyAndroid::UpdateProcessRankAndClearTimer(
    const PageNode* page_node) {
  UpdateProcessRank(page_node);
  visibility_timers_.erase(page_node);
}

void ProcessRankPolicyAndroid::UpdateProcessRank(const PageNode* page_node) {
  content::ChildProcessImportance importance = CalculateRank(page_node);

  const base::WeakPtr<content::WebContents> web_contents =
      page_node->GetWebContents();
  CHECK(web_contents, base::NotFatalUntil::M140);
  if (web_contents) {
    content::ChildProcessImportance subframe_importance =
        content::ChildProcessImportance::NORMAL;
    if (base::FeatureList::IsEnabled(features::kSubframeImportance) &&
        importance >= content::ChildProcessImportance::PERCEPTIBLE) {
      if (is_perceptible_importance_supported_) {
        subframe_importance = content::ChildProcessImportance::PERCEPTIBLE;
      } else if (base::FeatureList::IsEnabled(
                     chrome::android::kProtectedTabsAndroid) &&
                 chrome::android::kFallbackToModerateParam.Get()) {
        subframe_importance = content::ChildProcessImportance::MODERATE;
      }
    }
    web_contents->SetPrimaryPageImportance(importance, subframe_importance);
  }
}

content::ChildProcessImportance ProcessRankPolicyAndroid::CalculateRank(
    const PageNode* page_node) {
  if (page_node->IsVisible()) {
    // On Android visibility is updated synchronously on tab switch, but focus
    // is updated asynchronously. Focused status should be checked only if it is
    // visible.
    if (page_node->IsFocused() ||
        !base::FeatureList::IsEnabled(
            chrome::android::kChangeUnfocusedPriority)) {
      return content::ChildProcessImportance::IMPORTANT;
    }
    return content::ChildProcessImportance::MODERATE;
  }

  if (!base::FeatureList::IsEnabled(chrome::android::kProtectedTabsAndroid) ||
      !is_perceptible_importance_supported_) {
    const PageLiveStateDecorator::Data* live_state_data =
        PageLiveStateDecorator::Data::FromPageNode(page_node);
    if (live_state_data && live_state_data->IsActiveTab()) {
      return content::ChildProcessImportance::MODERATE;
    }
  }

  if (base::FeatureList::IsEnabled(chrome::android::kProtectedTabsAndroid)) {
    DiscardEligibilityPolicy* eligibility_policy =
        DiscardEligibilityPolicy::GetFromGraph(GetOwningGraph());
    CHECK(eligibility_policy);
    base::TimeDelta minimum_time_in_background;
    if (base::FeatureList::IsEnabled(
            chrome::android::kProtectRecentlyVisibleTab)) {
      minimum_time_in_background = base::Seconds(
          chrome::android::kProtectRecentlyVisibleTabDuration.Get());
    }
    if (eligibility_policy->CanDiscard(
            page_node, DiscardEligibilityPolicy::DiscardReason::PROACTIVE,
            minimum_time_in_background) != CanDiscardResult::kEligible) {
      if (is_perceptible_importance_supported_) {
        return content::ChildProcessImportance::PERCEPTIBLE;
      } else if (chrome::android::kFallbackToModerateParam.Get()) {
        return content::ChildProcessImportance::MODERATE;
      }
    }
  }

  return content::ChildProcessImportance::NORMAL;
}

}  // namespace performance_manager::policies

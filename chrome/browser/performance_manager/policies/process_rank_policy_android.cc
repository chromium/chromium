// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/process_rank_policy_android.h"

#include <optional>
#include <utility>

#include "base/android/android_info.h"
#include "base/android/device_info.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "content/public/browser/android/child_process_importance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "components/performance_manager/public/graph/frame_node.h"
#include "content/public/common/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"  // nogncheck
#else
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"  // nogncheck
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

namespace performance_manager::policies {

#if BUILDFLAG(ENABLE_GUEST_VIEW)

namespace {
bool IsWebViewInWebUI(const PageNode* page_node) {
  const auto web_contents = page_node->GetWebContents();
  if (!web_contents) {
    return false;
  }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  const FrameNode* embedder_frame = page_node->GetEmbedderFrameNode();
  if (!embedder_frame ||
      !embedder_frame->GetPageNode()->GetMainFrameUrl().SchemeIs(
          content::kChromeUIScheme)) {
    return false;
  }
  return extensions::WebViewGuest::FromWebContents(web_contents.get());
#else
  // SlimWebViews are only used in WebUIs.
  return guest_view::SlimWebViewGuest::FromWebContents(web_contents.get());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

}  // namespace

// WebViewUpdater tracks the relationship between a SlimWebView guest and its
// embedder. SlimWebViews are only used in WebUIs and
// are crucial to the WebUI functions. Therefore, WebViewUpdater ensures to
// update the process rank of the SlimWebView when thethe visibility or focus
// of the embedder page changes.
// Being a PageNodeObserver, it will be called for every page node change, even
// if the observer returns early.
// This is fine since the number of SlimWebView guest pages is expected to be
// small.
class WebViewUpdater : public PageNodeObserver,
                       public NodeAttachedDataImpl<WebViewUpdater> {
 public:
  explicit WebViewUpdater(const PageNode* guest_node)
      : guest_node_(guest_node),
        policy_(
            ProcessRankPolicyAndroid::GetFromGraph(guest_node_->GetGraph())) {
    guest_node_->GetGraph()->AddPageNodeObserver(this);
    policy_->UpdateProcessRank(guest_node_);
  }

  ~WebViewUpdater() override {
    guest_node_->GetGraph()->RemovePageNodeObserver(this);
  }

  WebViewUpdater(const WebViewUpdater&) = delete;
  WebViewUpdater& operator=(const WebViewUpdater&) = delete;

  // PageNodeObserver implementation:
  void OnEmbedderFrameNodeChanged(const PageNode* page_node,
                                  const FrameNode* previous_embedder) override {
    if (page_node != guest_node_) {
      return;
    }
    const PageNode* previous_embedder_node =
        previous_embedder ? previous_embedder->GetPageNode() : nullptr;
    if (GetEmbedderNode() != previous_embedder_node) {
      policy_->UpdateProcessRank(guest_node_);
    }
  }

  void OnIsVisibleChanged(const PageNode* page_node) override {
    if (page_node == GetEmbedderNode()) {
      policy_->UpdateProcessRank(guest_node_);
    }
  }

  void OnIsFocusedChanged(const PageNode* page_node) override {
    if (page_node == GetEmbedderNode()) {
      policy_->UpdateProcessRank(guest_node_);
    }
  }

 private:
  const PageNode* GetEmbedderNode() const {
    const FrameNode* embedder_frame = guest_node_->GetEmbedderFrameNode();
    return embedder_frame ? embedder_frame->GetPageNode() : nullptr;
  }

  // The WebViewUpdater is attached to the guest node and the policy guaranteed
  // to outlive the guest node.
  const raw_ptr<const PageNode> guest_node_;
  const raw_ptr<ProcessRankPolicyAndroid> policy_;
};

#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

namespace {

bool IsChangeUnfocusedPriorityEnabled() {
  return base::android::device_info::is_desktop() ||
         base::FeatureList::IsEnabled(
             chrome::android::kChangeUnfocusedPriority);
}

bool IsProtectRecentlyVisibleTabEnabled() {
  return base::android::device_info::is_desktop() ||
         base::FeatureList::IsEnabled(
             chrome::android::kProtectRecentlyVisibleTab);
}

}  // namespace

ProcessRankPolicyAndroid::ProcessRankPolicyAndroid()
    : ProcessRankPolicyAndroid(content::IsNotPerceptibleImportanceSupported()) {
}

ProcessRankPolicyAndroid::ProcessRankPolicyAndroid(
    bool is_perceptible_importance_supported)
    : is_perceptible_importance_supported_(
          is_perceptible_importance_supported) {}

ProcessRankPolicyAndroid::~ProcessRankPolicyAndroid() = default;

void ProcessRankPolicyAndroid::OnGuestViewAssociated(
    const PageNode* page_node) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  if (IsWebViewInWebUI(page_node)) {
    WebViewUpdater::GetOrCreate(page_node);
  }
#endif
}

void ProcessRankPolicyAndroid::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph->AddPageNodeObserver(this);
}

void ProcessRankPolicyAndroid::OnTakenFromGraph(Graph* graph) {
  graph->RemovePageNodeObserver(this);
  if (IsProtectRecentlyVisibleTabEnabled()) {
    visibility_timers_.clear();
  }
}

void ProcessRankPolicyAndroid::OnPageNodeAdded(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)->AddObserver(
      this);
  UpdateProcessRank(page_node);

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  if (IsWebViewInWebUI(page_node)) {
    WebViewUpdater::GetOrCreate(page_node);
  }
#endif
}

void ProcessRankPolicyAndroid::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->RemoveObserver(this);
  if (IsProtectRecentlyVisibleTabEnabled()) {
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
  if (IsProtectRecentlyVisibleTabEnabled()) {
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

void ProcessRankPolicyAndroid::OnEmbedderFrameNodeChanged(
    const PageNode* page_node,
    const FrameNode* previous_embedder) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  if (IsWebViewInWebUI(page_node)) {
    WebViewUpdater::GetOrCreate(page_node);
  }
#endif
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
  CHECK(web_contents);
  content::ChildProcessImportance subframe_importance =
      content::ChildProcessImportance::NORMAL;
  if (importance >= content::ChildProcessImportance::NOT_PERCEPTIBLE) {
    if (is_perceptible_importance_supported_) {
      subframe_importance = content::ChildProcessImportance::NOT_PERCEPTIBLE;
    } else if (base::FeatureList::IsEnabled(
                   chrome::android::kProtectedTabsAndroid) &&
               chrome::android::kFallbackToModerateParam.Get()) {
      subframe_importance = content::ChildProcessImportance::MODERATE;
    }
  }
  web_contents->SetPrimaryPageImportance(importance, subframe_importance);
}

content::ChildProcessImportance ProcessRankPolicyAndroid::CalculateRank(
    const PageNode* page_node) {
  const PageNode* node_to_check_visibility_and_focus = page_node;
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // In all scenarios where a webview is used in a WebUI, the webview is central
  // to the parent UI, therefore it should inherit the visibility and focus of
  // the parent page node.
  const FrameNode* embedder_frame = page_node->GetEmbedderFrameNode();
  if (embedder_frame && IsWebViewInWebUI(page_node)) {
    node_to_check_visibility_and_focus = embedder_frame->GetPageNode();
  }
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

  if (node_to_check_visibility_and_focus->IsVisible()) {
    // On Android visibility is updated synchronously on tab switch, but focus
    // is updated asynchronously. Focused status should be checked only if it is
    // visible.
    // When the page is embedded, the focus might be in the embedder or the
    // embeddee. In either case, the page should be considered important.
    if (!IsChangeUnfocusedPriorityEnabled() ||
        node_to_check_visibility_and_focus->IsFocused() ||
        page_node->IsFocused()) {
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
    if (eligibility_policy->CanDiscard(
            page_node, DiscardEligibilityPolicy::DiscardReason::PROACTIVE) !=
        CanDiscardResult::kEligible) {
      if (is_perceptible_importance_supported_) {
        return content::ChildProcessImportance::NOT_PERCEPTIBLE;
      } else if (chrome::android::kFallbackToModerateParam.Get()) {
        return content::ChildProcessImportance::MODERATE;
      }
    }
  }

  return content::ChildProcessImportance::NORMAL;
}

}  // namespace performance_manager::policies

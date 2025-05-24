// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"

#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"

namespace performance_manager::policies {

namespace {

BASE_FEATURE(kIgnoreDiscardAttemptMarker,
             "IgnoreDiscardAttemptMarker",
             base::FEATURE_DISABLED_BY_DEFAULT);

// NodeAttachedData used to indicate that there's already been an attempt to
// discard a PageNode.
class DiscardAttemptMarker
    : public ExternalNodeAttachedDataImpl<DiscardAttemptMarker> {
 public:
  explicit DiscardAttemptMarker(const PageNodeImpl* page_node) {}
  ~DiscardAttemptMarker() override = default;
};

const char kDescriberName[] = "DiscardEligibilityPolicy";

const PageLiveStateDecorator::Data* GetPageNodeLiveStateData(
    const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node);
}

}  // namespace

PageNodeSortProxy::PageNodeSortProxy(
    base::WeakPtr<const PageNode> page_node,
    CanDiscardResult can_discard_result,
    bool is_visible,
    bool is_focused,
    base::TimeTicks last_visibility_change_time)
    : page_node_(std::move(page_node)),
      can_discard_result_(can_discard_result),
      is_visible_(is_visible),
      is_focused_(is_focused),
      last_visibility_change_time_(last_visibility_change_time) {}

PageNodeSortProxy::PageNodeSortProxy(PageNodeSortProxy&&) = default;
PageNodeSortProxy& PageNodeSortProxy::operator=(PageNodeSortProxy&&) = default;

PageNodeSortProxy::~PageNodeSortProxy() = default;

DiscardEligibilityPolicy::DiscardEligibilityPolicy() = default;
DiscardEligibilityPolicy::~DiscardEligibilityPolicy() = default;

void DiscardEligibilityPolicy::SetNoDiscardPatternsForProfile(
    const std::string& browser_context_id,
    const std::vector<std::string>& patterns) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<url_matcher::URLMatcher>& entry =
      profiles_no_discard_patterns_[browser_context_id];
  entry = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::util::AddAllowFiltersWithLimit(entry.get(), patterns);
  if (opt_out_policy_changed_callback_) {
    opt_out_policy_changed_callback_.Run(browser_context_id);
  }
}

void DiscardEligibilityPolicy::ClearNoDiscardPatternsForProfile(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  profiles_no_discard_patterns_.erase(browser_context_id);
  if (opt_out_policy_changed_callback_) {
    opt_out_policy_changed_callback_.Run(browser_context_id);
  }
}

// static
void DiscardEligibilityPolicy::AddDiscardAttemptMarker(PageNode* page_node) {
  DiscardAttemptMarker::GetOrCreate(PageNodeImpl::FromNode(page_node));
}

// static
void DiscardEligibilityPolicy::RemovesDiscardAttemptMarkerForTesting(
    PageNode* page_node) {
  DiscardAttemptMarker::Destroy(PageNodeImpl::FromNode(page_node));
}

void DiscardEligibilityPolicy::SetOptOutPolicyChangedCallback(
    base::RepeatingCallback<void(std::string_view)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  opt_out_policy_changed_callback_ = std::move(callback);
}

void DiscardEligibilityPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddPageNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void DiscardEligibilityPolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemovePageNodeObserver(this);
}

// NOTE: This is used by ProcessRankPolicyAndroid. If you add a new condition to
// this, you need to add an observer callback to ProcessRankPolicyAndroid as
// well.
CanDiscardResult DiscardEligibilityPolicy::CanDiscard(
    const PageNode* page_node,
    DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background,
    std::vector<CannotDiscardReason>* cannot_discard_reasons) const {
  auto add_reason = [&](CannotDiscardReason reason) {
    if (cannot_discard_reasons) {
      cannot_discard_reasons->push_back(reason);
    }
  };

  // Don't discard pages which aren't tabs.
  if (page_node->GetType() != PageType::kTab) {
    add_reason(CannotDiscardReason::kNotATab);
    return CanDiscardResult::kDisallowed;
  }

  // Don't discard tabs for which discarding has already been attempted.
  if (DiscardAttemptMarker::Get(PageNodeImpl::FromNode(page_node)) &&
      !base::FeatureList::IsEnabled(kIgnoreDiscardAttemptMarker)) {
    add_reason(CannotDiscardReason::kDiscardAttempted);
    return CanDiscardResult::kDisallowed;
  }

  // Don't discard tabs that don't have a main frame (restored tab which is not
  // loaded yet, discarded tab, crashed tab).
  if (!page_node->GetMainFrameNode()) {
    add_reason(CannotDiscardReason::kNoMainFrame);
    return CanDiscardResult::kDisallowed;
  }

  const auto* live_state_data = GetPageNodeLiveStateData(page_node);

  // Don't discard tabs that are already discarded, as that will fail.
  if (live_state_data && live_state_data->IsDiscarded()) {
    add_reason(CannotDiscardReason::kAlreadyDiscarded);
    return CanDiscardResult::kDisallowed;
  }

  bool is_proactive_or_suggested;
  switch (discard_reason) {
    case DiscardReason::EXTERNAL:
    case DiscardReason::FROZEN_WITH_GROWING_MEMORY:
      // Always allow discards.
      return CanDiscardResult::kEligible;
    case DiscardReason::URGENT:
      is_proactive_or_suggested = false;
      break;
    case DiscardReason::PROACTIVE:
    case DiscardReason::SUGGESTED:
      is_proactive_or_suggested = true;
      break;
  }

  CanDiscardResult result = CanDiscardResult::kEligible;
  auto add_reason_and_update_result = [&](CannotDiscardReason reason,
                                          CanDiscardResult new_result) {
    if (cannot_discard_reasons) {
      cannot_discard_reasons->push_back(reason);
    }
    result = std::underlying_type_t<CanDiscardResult>(result) <
                     std::underlying_type_t<CanDiscardResult>(new_result)
                 ? new_result
                 : result;
  };

  if (page_node->IsVisible()) {
    add_reason_and_update_result(CannotDiscardReason::kVisible,
                                 CanDiscardResult::kProtected);
  } else if ((base::TimeTicks::Now() -
              page_node->GetLastVisibilityChangeTime()) <
             minimum_time_in_background) {
    add_reason_and_update_result(CannotDiscardReason::kRecentlyVisible,
                                 CanDiscardResult::kProtected);
  }

  // Don't discard tabs that are playing or have recently played audio.
  if (page_node->IsAudible()) {
    add_reason_and_update_result(CannotDiscardReason::kAudible,
                                 CanDiscardResult::kProtected);
  } else if (page_node->GetTimeSinceLastAudibleChange().value_or(
                 base::TimeDelta::Max()) < kTabAudioProtectionTime) {
    add_reason_and_update_result(CannotDiscardReason::kRecentlyAudible,
                                 CanDiscardResult::kProtected);
  }

  // Don't discard pages that are displaying content in picture-in-picture.
  if (page_node->HasPictureInPicture()) {
    add_reason_and_update_result(CannotDiscardReason::kPictureInPicture,
                                 CanDiscardResult::kProtected);
  }

  // Do not discard PDFs as they might contain entry that is not saved and they
  // don't remember their scrolling positions. See crbug.com/547286 and
  // crbug.com/65244.
  if (page_node->GetContentsMimeType() == "application/pdf") {
    add_reason_and_update_result(CannotDiscardReason::kPdf,
                                 CanDiscardResult::kProtected);
  }

  const GURL& main_frame_url = page_node->GetMainFrameUrl();
  if (!main_frame_url.is_valid() || main_frame_url.is_empty()) {
    add_reason_and_update_result(CannotDiscardReason::kInvalidURL,
                                 CanDiscardResult::kProtected);
  }

  // Only discard http(s) pages and internal pages to make sure that we don't
  // discard extensions or other PageNode that don't correspond to a tab.
  //
  // TODO(crbug.com/40910297): Due to a state tracking bug, sometimes there are
  // two frames marked "current". In that case GetMainFrameNode() returns an
  // arbitrary one, which may not have the url set correctly. Therefore, use
  // GetMainFrameUrl() for the url.
  bool is_web_page_or_internal_or_data_page =
      main_frame_url.SchemeIsHTTPOrHTTPS() ||
      main_frame_url.SchemeIs("chrome") ||
      main_frame_url.SchemeIs(url::kDataScheme);
  if (!is_web_page_or_internal_or_data_page) {
    add_reason_and_update_result(CannotDiscardReason::kNotWebOrInternal,
                                 CanDiscardResult::kProtected);
  }

  // The enterprise policy to except pages from discarding applies to both
  // proactive and urgent discards.
  if (IsPageOptedOutOfDiscarding(page_node->GetBrowserContextID(),
                                 main_frame_url)) {
    add_reason_and_update_result(CannotDiscardReason::kOptedOut,
                                 CanDiscardResult::kProtected);
  }

  if (is_proactive_or_suggested &&
      page_node->GetNotificationPermissionStatus() ==
          blink::mojom::PermissionStatus::GRANTED) {
    add_reason_and_update_result(CannotDiscardReason::kNotificationsEnabled,
                                 CanDiscardResult::kProtected);
  }

  // The live state data won't be available if none of these events ever
  // happened on the page.
  if (live_state_data) {
    // Don't discard the page if an extension is protecting it from discards.
    if (!live_state_data->IsAutoDiscardable()) {
      add_reason_and_update_result(CannotDiscardReason::kExtensionProtected,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsCapturingVideo()) {
      add_reason_and_update_result(CannotDiscardReason::kCapturingVideo,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsCapturingAudio()) {
      add_reason_and_update_result(CannotDiscardReason::kCapturingAudio,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsBeingMirrored()) {
      add_reason_and_update_result(CannotDiscardReason::kBeingMirrored,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsCapturingWindow()) {
      add_reason_and_update_result(CannotDiscardReason::kCapturingWindow,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsCapturingDisplay()) {
      add_reason_and_update_result(CannotDiscardReason::kCapturingDisplay,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsConnectedToBluetoothDevice()) {
      add_reason_and_update_result(CannotDiscardReason::kConnectedToBluetooth,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsConnectedToUSBDevice()) {
      add_reason_and_update_result(CannotDiscardReason::kConnectedToUSB,
                                   CanDiscardResult::kProtected);
    }
    // Don't discard the active tab in any window, even if the window is not
    // visible. Otherwise the user would see a blank page when the window
    // becomes visible again, as the tab isn't reloaded until they click on it.
    if (live_state_data->IsActiveTab()) {
      add_reason_and_update_result(CannotDiscardReason::kActiveTab,
                                   CanDiscardResult::kProtected);
    }
    // Pinning a tab is a strong signal the user wants to keep it.
    if (live_state_data->IsPinnedTab()) {
      add_reason_and_update_result(CannotDiscardReason::kPinnedTab,
                                   CanDiscardResult::kProtected);
    }
    // Don't discard pages with devtools attached, because when it's restored
    // the devtools window won't come back. The user may be monitoring the page
    // in the background with devtools.
    if (live_state_data->IsDevToolsOpen()) {
      add_reason_and_update_result(CannotDiscardReason::kDevToolsOpen,
                                   CanDiscardResult::kProtected);
    }
    if (is_proactive_or_suggested &&
        live_state_data->UpdatedTitleOrFaviconInBackground()) {
      add_reason_and_update_result(CannotDiscardReason::kBackgroundActivity,
                                   CanDiscardResult::kProtected);
    }
  }

  // `HadUserEdits()` is currently a superset of `HadFormInteraction()` but
  // that may change so check both here (the check is not expensive).
  if (page_node->HadFormInteraction()) {
    add_reason_and_update_result(CannotDiscardReason::kFormInteractions,
                                 CanDiscardResult::kProtected);
  }

  if (page_node->HadUserEdits()) {
    add_reason_and_update_result(CannotDiscardReason::kUserEdits,
                                 CanDiscardResult::kProtected);
  }

  return result;
}

bool DiscardEligibilityPolicy::IsPageOptedOutOfDiscarding(
    const std::string& browser_context_id,
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = profiles_no_discard_patterns_.find(browser_context_id);
  if (it == profiles_no_discard_patterns_.end()) {
    // There can be a narrow window between profile creation and when prefs are
    // read, which is when `profiles_no_discard_patterns_` is populated. During
    // that time assume that a page might be opted out of discarding.
    return true;
  }
  return !it->second->MatchURL(url).empty();
}

void DiscardEligibilityPolicy::OnMainFrameDocumentChanged(
    const PageNode* page_node) {
  // When activated a discarded tab will re-navigate, instantiating a new
  // document. Ensure the DiscardAttemptMarker is cleared in these cases to
  // ensure a given tab remains eligible for discarding.
  DiscardAttemptMarker::Destroy(PageNodeImpl::FromNode(page_node));
}

base::Value::Dict DiscardEligibilityPolicy::DescribePageNodeData(
    const PageNode* node) const {
  auto can_discard = [this, node](DiscardReason discard_reason) {
    switch (this->CanDiscard(node, discard_reason, base::TimeDelta())) {
      case CanDiscardResult::kEligible:
        return "eligible";
      case CanDiscardResult::kProtected:
        return "protected";
      case CanDiscardResult::kDisallowed:
        return "disallowed";
    }
  };

  base::Value::Dict ret;
  ret.Set("can_urgently_discard", can_discard(DiscardReason::URGENT));
  ret.Set("can_proactively_discard", can_discard(DiscardReason::PROACTIVE));
  if (!node->GetMainFrameUrl().is_empty()) {
    ret.Set("opted_out", IsPageOptedOutOfDiscarding(node->GetBrowserContextID(),
                                                    node->GetMainFrameUrl()));
  }

  return ret;
}

}  // namespace performance_manager::policies

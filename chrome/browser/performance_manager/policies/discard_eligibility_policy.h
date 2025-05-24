// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_DISCARD_ELIGIBILITY_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_DISCARD_ELIGIBILITY_POLICY_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/cannot_discard_reason.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace url_matcher {
class URLMatcher;
}  // namespace url_matcher

namespace performance_manager::policies {

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
inline constexpr base::TimeDelta kNonVisiblePagesUrgentProtectionTime =
    base::TimeDelta();
#else
// Time during which non visible pages are protected from urgent discarding
// (not on ChromeOS).
inline constexpr base::TimeDelta kNonVisiblePagesUrgentProtectionTime =
    base::Minutes(10);
#endif

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/412839833): kTabAudioProtectionTime may be needed on Android
// as well.
inline constexpr base::TimeDelta kTabAudioProtectionTime = base::TimeDelta();
#else
// Time during which a tab cannot be discarded after having played audio.
inline constexpr base::TimeDelta kTabAudioProtectionTime = base::Minutes(1);
#endif

// Whether a page can be discarded.
enum class CanDiscardResult {
  // The page can be discarded. The user should experience minimal disruption
  // from discarding.
  kEligible,
  // The page can be discarded. The user will likely find discarding disruptive.
  kProtected,
  // The page cannot be discarded.
  kDisallowed,
};

// Caches page node properties to facilitate sorting.
class PageNodeSortProxy {
 public:
  PageNodeSortProxy(base::WeakPtr<const PageNode> page_node,
                    CanDiscardResult can_discard_result,
                    bool is_visible,
                    bool is_focused,
                    base::TimeTicks last_visibility_change_time);
  PageNodeSortProxy(PageNodeSortProxy&&);
  PageNodeSortProxy& operator=(PageNodeSortProxy&&);
  ~PageNodeSortProxy();

  base::WeakPtr<const PageNode> page_node() const { return page_node_; }
  bool is_disallowed() const {
    return can_discard_result_ == CanDiscardResult::kDisallowed;
  }
  bool is_protected() const {
    return can_discard_result_ == CanDiscardResult::kProtected;
  }
  bool is_visible() const { return is_visible_; }
  bool is_focused() const { return is_focused_; }
  base::TimeTicks last_visibility_change_time() const {
    return last_visibility_change_time_;
  }

  // Returns true if the rhs is more important.
  bool operator<(const PageNodeSortProxy& rhs) const {
    if (is_disallowed() != rhs.is_disallowed()) {
      return rhs.is_disallowed();
    }
    if (is_focused_ != rhs.is_focused_) {
      return rhs.is_focused_;
    }
    if (is_visible_ != rhs.is_visible_) {
      return rhs.is_visible_;
    }
    if (is_protected() != rhs.is_protected()) {
      return rhs.is_protected();
    }
    return last_visibility_change_time_ < rhs.last_visibility_change_time_;
  }

 private:
  base::WeakPtr<const PageNode> page_node_;
  CanDiscardResult can_discard_result_;
  bool is_visible_;
  bool is_focused_;
  base::TimeTicks last_visibility_change_time_;
};

// DiscardEligibilityPolicy decides which PageNode is eligigle for tab
// discarding.
class DiscardEligibilityPolicy
    : public GraphOwnedAndRegistered<DiscardEligibilityPolicy>,
      public NodeDataDescriberDefaultImpl,
      public PageNodeObserver {
 public:
  // Export discard reason in the public interface.
  using DiscardReason = ::mojom::LifecycleUnitDiscardReason;

  DiscardEligibilityPolicy();
  ~DiscardEligibilityPolicy() override;
  DiscardEligibilityPolicy(const DiscardEligibilityPolicy& other) = delete;
  DiscardEligibilityPolicy& operator=(const DiscardEligibilityPolicy&) = delete;

  // PageNodeObserver:
  void OnMainFrameDocumentChanged(const PageNode* page_node) override;

  base::WeakPtr<DiscardEligibilityPolicy> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void SetNoDiscardPatternsForProfile(const std::string& browser_context_id,
                                      const std::vector<std::string>& patterns);
  void ClearNoDiscardPatternsForProfile(const std::string& browser_context_id);

  // Indicates if `page_node` can be urgently discarded, using a list of
  // criteria depending on `discard_reason`. If `minimum_time_in_background` is
  // non-zero, the page will not be discarded if it has not spent at least
  // `minimum_time_in_background` in the not-visible state.
  CanDiscardResult CanDiscard(
      const PageNode* page_node,
      DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background =
          kNonVisiblePagesUrgentProtectionTime,
      std::vector<CannotDiscardReason>* cannot_discard_reasons = nullptr) const;

  // This must be called from PageDiscardingHelper or from test only.
  static void AddDiscardAttemptMarker(PageNode* page_node);
  static void RemovesDiscardAttemptMarkerForTesting(PageNode* page_node);

  // Sets an additional callback that should be invoked whenever the
  // SetNoDiscardPatternsForProfile() or ClearNoDiscardPatternsForProfile()
  // methosd is called, with the method's `browser_context_id` argument.
  void SetOptOutPolicyChangedCallback(
      base::RepeatingCallback<void(std::string_view)> callback);

  bool IsPageOptedOutOfDiscarding(const std::string& browser_context_id,
                                  const GURL& url) const;

 private:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber implementation:
  base::Value::Dict DescribePageNodeData(const PageNode* node) const override;

  std::map<std::string, std::unique_ptr<url_matcher::URLMatcher>>
      profiles_no_discard_patterns_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::RepeatingCallback<void(std::string_view)>
      opt_out_policy_changed_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DiscardEligibilityPolicy> weak_factory_{this};
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_DISCARD_ELIGIBILITY_POLICY_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_DISCARDING_HELPER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_DISCARDING_HELPER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "components/memory_pressure/reclaim_target.h"
#include "components/memory_pressure/unnecessary_discard_monitor.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace url_matcher {
class URLMatcher;
}  // namespace url_matcher

namespace performance_manager {

namespace mechanism {
class PageDiscarder;
}  // namespace mechanism

namespace policies {

#if BUILDFLAG(IS_CHROMEOS)
constexpr base::TimeDelta kNonVisiblePagesUrgentProtectionTime =
    base::TimeDelta();
#else
// Time during which non visible pages are protected from urgent discarding
// (not on ChromeOS).
constexpr base::TimeDelta kNonVisiblePagesUrgentProtectionTime =
    base::Minutes(10);
#endif

// Time during which a tab cannot be discarded after having played audio.
constexpr base::TimeDelta kTabAudioProtectionTime = base::Minutes(1);

// Caches page node properties to facilitate sorting.
class PageNodeSortProxy {
 public:
  PageNodeSortProxy(const PageNode* page_node,
                    bool is_marked,
                    bool is_visible,
                    bool is_protected,
                    bool is_focused,
                    base::TimeDelta last_visible)
      : page_node_(page_node),
        is_marked_(is_marked),
        is_visible_(is_visible),
        is_protected_(is_protected),
        is_focused_(is_focused),
        last_visible_(last_visible) {}

  const PageNode* page_node() const { return page_node_; }
  bool is_marked() const { return is_marked_; }
  bool is_protected() const { return is_protected_; }
  bool is_visible() const { return is_visible_; }
  bool is_focused() const { return is_focused_; }
  base::TimeDelta last_visible() const { return last_visible_; }

  // Returns true if the rhs is more important.
  bool operator<(const PageNodeSortProxy& rhs) const {
    if (is_marked_ != rhs.is_marked_) {
      return rhs.is_marked_;
    }
    if (is_visible_ != rhs.is_visible_) {
      return rhs.is_visible_;
    }
    if (is_protected_ != rhs.is_protected_) {
      return rhs.is_protected_;
    }
    return last_visible_ > rhs.last_visible_;
  }

 private:
  raw_ptr<const PageNode> page_node_;
  bool is_marked_;
  bool is_visible_;
  bool is_protected_;
  bool is_focused_;
  // Delta between current time and last visibility change time.
  base::TimeDelta last_visible_;
};

// Helper class to be used by the policies that want to discard tabs.
//
// This is a GraphRegistered object and should be accessed via
// PageDiscardingHelper::GetFromGraph(graph()).
class PageDiscardingHelper
    : public GraphOwnedAndRegistered<PageDiscardingHelper>,
      public NodeDataDescriberDefaultImpl {
 public:
  enum class CanDiscardResult {
    // Discarding eligible nodes is hard to notice for user.
    kEligible,
    // Discarding protected nodes is noticeable to user.
    kProtected,
    // Marked nodes can never be discarded.
    kMarked,
  };

  // Export discard reason in the public interface.
  using DiscardReason = ::mojom::LifecycleUnitDiscardReason;
  // DiscardCallback passes the time of first discarding is done.
  // If discarding fails or there is no candidate for discarding, this passes
  // nullopt.
  using DiscardCallback =
      base::OnceCallback<void(std::optional<base::TimeTicks>)>;

  PageDiscardingHelper();
  ~PageDiscardingHelper() override;
  PageDiscardingHelper(const PageDiscardingHelper& other) = delete;
  PageDiscardingHelper& operator=(const PageDiscardingHelper&) = delete;

  // Selects a tab to discard and posts to the UI thread to discard it. This
  // will try to discard a tab until there's been a successful discard or until
  // there's no more discard candidate.
  // `minimum_time_in_background` is passed to `CanDiscard()`, see the comment
  // there about its usage.
  void DiscardAPage(DiscardCallback post_discard_cb,
                    DiscardReason discard_reason,
                    base::TimeDelta minimum_time_in_background =
                        kNonVisiblePagesUrgentProtectionTime);

  // Discards multiple tabs to meet the reclaim target based and posts to the UI
  // thread to discard these tabs. Retries discarding if all discardings in the
  // UI thread fail. If |reclaim_target_kb| is nullopt, only discard one tab. If
  // |discard_protected_tabs| is true, protected tabs (CanDiscard() returns
  // kProtected) can also be discarded.
  // `minimum_time_in_background` is passed to `CanDiscard()`, see the comment
  // there about its usage.
  void DiscardMultiplePages(
      std::optional<memory_pressure::ReclaimTarget> reclaim_target,
      bool discard_protected_tabs,
      DiscardCallback post_discard_cb,
      DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background =
          kNonVisiblePagesUrgentProtectionTime);

  void ImmediatelyDiscardMultiplePages(
      const std::vector<const PageNode*>& page_nodes,
      DiscardReason discard_reason,
      DiscardCallback post_discard_cb = base::DoNothing());

  void SetNoDiscardPatternsForProfile(const std::string& browser_context_id,
                                      const std::vector<std::string>& patterns);
  void ClearNoDiscardPatternsForProfile(const std::string& browser_context_id);

  void SetMockDiscarderForTesting(
      std::unique_ptr<mechanism::PageDiscarder> discarder);

  // Indicates if `page_node` can be urgently discarded, using a list of
  // criteria depending on `discard_reason`. If `minimum_time_in_background` is
  // non-zero, the page will not be discarded if it has not spent at least
  // `minimum_time_in_background` in the not-visible state.
  CanDiscardResult CanDiscard(const PageNode* page_node,
                              DiscardReason discard_reason,
                              base::TimeDelta minimum_time_in_background =
                                  kNonVisiblePagesUrgentProtectionTime) const;

  static void AddDiscardAttemptMarkerForTesting(PageNode* page_node);
  static void RemovesDiscardAttemptMarkerForTesting(PageNode* page_node);

 protected:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // Returns the PageLiveStateDecorator::Data associated with a PageNode.
  // Exposed and made virtual to allowed injecting some fake data in tests.
  virtual const PageLiveStateDecorator::Data* GetPageNodeLiveStateData(
      const PageNode* page_node) const;

 private:
  bool IsPageOptedOutOfDiscarding(const std::string& browser_context_id,
                                  const GURL& url) const;

  // NodeDataDescriber implementation:
  base::Value::Dict DescribePageNodeData(const PageNode* node) const override;

  // Called after each discard attempt. |success| will indicate whether or not
  // the attempt has been successful. |post_discard_cb| will be called once
  // there's been at least one successful discard or if there's no more discard
  // candidates.
  void PostDiscardAttemptCallback(
      std::optional<memory_pressure::ReclaimTarget> reclaim_target,
      bool discard_protected_tabs,
      DiscardCallback post_discard_cb,
      DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background,
      const std::vector<mechanism::PageDiscarder::DiscardEvent>&
          discard_events);

  // The mechanism used to do the actual discarding.
  std::unique_ptr<mechanism::PageDiscarder> page_discarder_;

  std::map<std::string, std::unique_ptr<url_matcher::URLMatcher>>
      profiles_no_discard_patterns_;

  memory_pressure::UnnecessaryDiscardMonitor unnecessary_discard_monitor_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PageDiscardingHelper> weak_factory_{this};
};

}  // namespace policies

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_DISCARDING_HELPER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_DISCARDING_HELPER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_DISCARDING_HELPER_H_

#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"
#include "chrome/browser/performance_manager/policies/cannot_discard_reason.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "components/memory_pressure/reclaim_target.h"
#include "components/memory_pressure/unnecessary_discard_monitor.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

namespace mechanism {
class PageDiscarder;
}  // namespace mechanism

namespace policies {

// Helper class to be used by the policies that want to discard tabs.
//
// This is a GraphRegistered object and should be accessed via
// PageDiscardingHelper::GetFromGraph(graph()).
//
// This requires DiscardEligibilityPolicy to be registered in the graph.
class PageDiscardingHelper
    : public GraphOwnedAndRegistered<PageDiscardingHelper>,
      public NodeDataDescriberDefaultImpl,
      public PageNodeObserver {
 public:
  // The result of page discard. The WebContents pointer is for
  // TabManager::DiscardTabByExtension.
  struct DiscardResult {
    // Time of the first successful discard, or nullopt if no successful
    // discard occurred.
    std::optional<base::TimeTicks> first_discard_time;
    // The WebContents of the first discarded tab after discard.
    raw_ptr<content::WebContents> first_content_after_discard = nullptr;
  };

  PageDiscardingHelper();
  ~PageDiscardingHelper() override;
  PageDiscardingHelper(const PageDiscardingHelper& other) = delete;
  PageDiscardingHelper& operator=(const PageDiscardingHelper&) = delete;

  // Selects and discards a tab. This will try to discard a tab until there's
  // been a successful discard or until there's no more discard candidate.
  // `minimum_time_in_background` is passed to `CanDiscard()`, see comment there
  // about usage.
  DiscardResult DiscardAPage(
      DiscardEligibilityPolicy::DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background =
          kNonVisiblePagesUrgentProtectionTime);

  // Selects and discards multiple tabs to meet the reclaim target. This will
  // keep trying again until there's been at least a single successful discard
  // or until there's no more discard candidate. If |reclaim_target_kb| is
  // nullopt, only discard one tab. If |discard_protected_tabs| is true,
  // protected tabs (CanDiscard() returns kProtected) can also be discarded.
  // `minimum_time_in_background` is passed to `CanDiscard()`, see comment there
  // about usage. Returns a time taken shortly after the first successful
  // discard, or nullopt if no successful discard occurred.
  std::optional<base::TimeTicks> DiscardMultiplePages(
      std::optional<memory_pressure::ReclaimTarget> reclaim_target,
      bool discard_protected_tabs,
      DiscardEligibilityPolicy::DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background =
          kNonVisiblePagesUrgentProtectionTime);

  // Immediately discards as many pages as possible in `page_nodes`.
  // `minimum_time_in_background` is passed to `CanDiscard()`, see comment there
  // about usage. Returns true if at least one page was successfully discarded.
  bool ImmediatelyDiscardMultiplePages(
      const std::vector<const PageNode*>& page_nodes,
      DiscardEligibilityPolicy::DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background = base::TimeDelta());

  void SetMockDiscarderForTesting(
      std::unique_ptr<mechanism::PageDiscarder> discarder);

 protected:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 private:
  // NodeDataDescriber implementation:
  base::Value::Dict DescribePageNodeData(const PageNode* node) const override;

  // Helper function so DiscardMultiplePages doesn't have to return the unused
  // WebContents pointer.
  DiscardResult DiscardMultiplePagesImpl(
      std::optional<memory_pressure::ReclaimTarget> reclaim_target,
      bool discard_protected_tabs,
      DiscardEligibilityPolicy::DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background =
          kNonVisiblePagesUrgentProtectionTime);

  // The mechanism used to do the actual discarding.
  std::unique_ptr<mechanism::PageDiscarder> page_discarder_;

  memory_pressure::UnnecessaryDiscardMonitor unnecessary_discard_monitor_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policies

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_DISCARDING_HELPER_H_

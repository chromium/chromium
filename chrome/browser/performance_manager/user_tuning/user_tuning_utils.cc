// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"

#include <optional>

#include "base/byte_count.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/common/content_features.h"

#if !BUILDFLAG(IS_ANDROID)
#include <algorithm>
#include <string>
#include <vector>

#include "chrome/browser/performance_manager/policies/cannot_discard_reason.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

namespace performance_manager::user_tuning {

bool IsRefreshRateThrottled() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  // This can be false in unit tests.
  if (!BatterySaverModeManager::HasInstance()) {
    return false;
  }

  return BatterySaverModeManager::GetInstance()->IsBatterySaverActive();
#endif
}

bool IsBatterySaverModeManagedByOS() {
#if BUILDFLAG(IS_CHROMEOS)
  return ash::features::IsBatterySaverAvailable();
#else
  return false;
#endif
}

base::ByteCount GetDiscardedMemoryEstimateForPage(const PageNode* node) {
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());

  return node->EstimatePrivateFootprintSize();
}

std::vector<std::string> GetCannotDiscardReasonsForPageNode(
    const PageNode* page_node) {
#if BUILDFLAG(IS_ANDROID)
  // Discarding on Android depends on kWebContentsDiscard.
  if (!base::FeatureList::IsEnabled(::features::kWebContentsDiscard)) {
    return {"not implemented"};
  }
#endif  // BUILDFLAG(IS_ANDROID)

  auto* discarding_helper = policies::DiscardEligibilityPolicy::GetFromGraph(
      PerformanceManager::GetGraph());
  CHECK(discarding_helper);
  CHECK(page_node);

  std::vector<policies::CannotDiscardReason> cannot_discard_reasons;
  discarding_helper->CanDiscard(
      page_node, policies::DiscardEligibilityPolicy::DiscardReason::PROACTIVE,
      policies::kNonVisiblePagesUrgentProtectionTime, &cannot_discard_reasons);

  std::vector<std::string> results;
  results.reserve(cannot_discard_reasons.size());  // Reserve space
  std::transform(cannot_discard_reasons.begin(), cannot_discard_reasons.end(),
                 std::back_inserter(results),
                 policies::CannotDiscardReasonToString);
  return results;
}

void DiscardPage(const PageNode* page_node,
                 ::mojom::LifecycleUnitDiscardReason reason,
                 bool ignore_minimum_time_in_background) {
  auto* discarding_helper = policies::PageDiscardingHelper::GetFromGraph(
      PerformanceManager::GetGraph());
  CHECK(discarding_helper);
  CHECK(page_node);
  discarding_helper->ImmediatelyDiscardMultiplePages(
      {page_node}, reason,
      ignore_minimum_time_in_background
          ? base::TimeDelta()
          : policies::kNonVisiblePagesUrgentProtectionTime);
}

void DiscardAnyPage(::mojom::LifecycleUnitDiscardReason reason,
                    bool ignore_minimum_time_in_background) {
  auto* discarding_helper = policies::PageDiscardingHelper::GetFromGraph(
      PerformanceManager::GetGraph());
  CHECK(discarding_helper);
  discarding_helper->DiscardAPage(
      reason, ignore_minimum_time_in_background
                  ? base::TimeDelta()
                  : policies::kNonVisiblePagesUrgentProtectionTime);
}

}  //  namespace performance_manager::user_tuning

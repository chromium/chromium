// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

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

uint64_t GetDiscardedMemoryEstimateForPage(const PageNode* node) {
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());

  return node->EstimatePrivateFootprintSize();
}

void GetDiscardedMemoryEstimateForWebContents(
    content::WebContents* web_contents,
    base::OnceCallback<void(uint64_t)> result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents);
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node,
             base::OnceCallback<void(uint64_t)> result_callback) {
            uint64_t estimate =
                page_node ? GetDiscardedMemoryEstimateForPage(page_node.get())
                          : 0;
            content::GetUIThreadTaskRunner({})->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(result_callback), estimate));
          },
          page_node, std::move(result_callback)));
}

std::vector<std::string> GetCannotDiscardReasonsForPageNode(
    const PageNode* page_node) {
#if BUILDFLAG(IS_ANDROID)
  return {};
#else
  policies::PageDiscardingHelper* discarding_helper =
      policies::PageDiscardingHelper::GetFromGraph(page_node->GetGraph());

  std::vector<policies::CannotDiscardReason> cannot_discard_reasons;
  discarding_helper->CanDiscard(
      page_node, policies::PageDiscardingHelper::DiscardReason::PROACTIVE,
      policies::kNonVisiblePagesUrgentProtectionTime, &cannot_discard_reasons);

  std::vector<std::string> results;
  results.reserve(cannot_discard_reasons.size());  // Reserve space
  std::transform(cannot_discard_reasons.begin(), cannot_discard_reasons.end(),
                 std::back_inserter(results),
                 policies::CannotDiscardReasonToString);
  return results;
#endif
}
}  //  namespace performance_manager::user_tuning

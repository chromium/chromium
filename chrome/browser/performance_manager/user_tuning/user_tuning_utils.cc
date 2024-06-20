// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"

#include <map>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace performance_manager::user_tuning {

namespace {

using PageContext = resource_attribution::PageContext;

std::vector<PageContextAndPmf> GetPrivateFootprintResults(
    const resource_attribution::QueryResultMap& results) {
  std::vector<PageContextAndPmf> private_footprints;
  private_footprints.reserve(results.size());
  for (const auto& [context, result] : results) {
    private_footprints.emplace_back(
        resource_attribution::AsContext<PageContext>(context),
        result.memory_summary_result->private_footprint_kb);
  }
  return private_footprints;
}

}  // namespace

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::features::IsBatterySaverAvailable();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsCrosBatterySaverAvailable();
#else
  return false;
#endif
}

void GetDiscardedMemoryEstimateForPageContext(
    PageContext page_context,
    base::OnceCallback<void(uint64_t)> result_callback) {
  // Wrap the context in a 1-element vector, and expect a 1-element result
  // vector. Get the PMF from the single element to pass to `result_callback`.
  GetDiscardedMemoryEstimateForPageContexts(
      {page_context},
      base::BindOnce([](std::vector<PageContextAndPmf> results) -> uint64_t {
        // Results may be empty on a measurement error.
        CHECK_LE(results.size(), 1u);
        return results.empty() ? 0 : results.front().second;
      }).Then(std::move(result_callback)));
}

void GetDiscardedMemoryEstimateForPageContexts(
    const std::vector<PageContext>& page_contexts,
    base::OnceCallback<void(std::vector<PageContextAndPmf>)> result_callback) {
  auto query = resource_attribution::QueryBuilder();
  query.AddResourceType(resource_attribution::ResourceType::kMemorySummary);
  for (const auto& page_context : page_contexts) {
    query.AddResourceContext(page_context);
  }
  query.QueryOnce(base::BindOnce(&GetPrivateFootprintResults)
                      .Then(std::move(result_callback)));
}

}  //  namespace performance_manager::user_tuning

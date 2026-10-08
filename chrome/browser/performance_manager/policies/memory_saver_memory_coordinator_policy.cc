// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/memory_saver_memory_coordinator_policy.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/memory_coordinator/traits.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"

namespace performance_manager::policies {

namespace {

using user_tuning::prefs::MemorySaverModeAggressiveness;

// Only stateful MemoryConsumers that support dynamic memory limits (such as
// resized in-memory caches) can meaningfully maintain a persistent percentage
// cap while Memory Saver Mode is active.
bool ShouldManageConsumer(uint32_t consumer_id,
                          std::string_view consumer_name,
                          base::MemoryConsumerTraits traits,
                          content::ProcessType process_type,
                          content::ChildProcessId child_process_id) {
  return traits.supports_memory_limit ==
             base::MemoryConsumerTraits::SupportsMemoryLimit::kYes &&
         traits.is_stateful == base::MemoryConsumerTraits::IsStateful::kYes;
}

base::MemoryLimit GetLimitForState(bool enabled,
                                   MemorySaverModeAggressiveness mode) {
  if (!enabled) {
    return base::MemoryLimit::Default();
  }
  switch (mode) {
    case MemorySaverModeAggressiveness::kConservative:
      return base::MemoryLimit::FromPercent(75);
    case MemorySaverModeAggressiveness::kMedium:
      return base::MemoryLimit::FromPercent(50);
    case MemorySaverModeAggressiveness::kAggressive:
      return base::MemoryLimit::FromPercent(25);
  }
}

}  // namespace

MemorySaverMemoryCoordinatorPolicy::MemorySaverMemoryCoordinatorPolicy(
    content::MemoryCoordinatorPolicyManager& manager)
    : content::PredicateMemoryCoordinatorPolicy(
          manager,
          base::BindRepeating(&ShouldManageConsumer)),
      registration_(manager, *this) {}

MemorySaverMemoryCoordinatorPolicy::~MemorySaverMemoryCoordinatorPolicy() =
    default;

void MemorySaverMemoryCoordinatorPolicy::UpdateState(
    bool enabled,
    MemorySaverModeAggressiveness mode) {
  const base::MemoryLimit new_limit = GetLimitForState(enabled, mode);
  if (new_limit == current_limit_) {
    return;
  }

  // Trigger an immediate memory release whenever the limit becomes stricter so
  // that existing caches shrink to the new cap right away instead of only
  // bounding future growth.
  const bool release_memory = new_limit < current_limit_;
  current_limit_ = new_limit;
  SetLimit(new_limit, release_memory);
}

}  // namespace performance_manager::policies

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/urgent_page_discarding_policy.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"

namespace performance_manager {
namespace policies {

UrgentPageDiscardingPolicy::UrgentPageDiscardingPolicy() = default;
UrgentPageDiscardingPolicy::~UrgentPageDiscardingPolicy() = default;

void UrgentPageDiscardingPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph_ = graph;
  DCHECK(!handling_memory_pressure_notification_);
  graph_->AddSystemNodeObserver(this);
  DCHECK(PageDiscardingHelper::GetFromGraph(graph_))
      << "A PageDiscardingHelper instance should be registered against the "
         "graph in order to use this policy.";
}

void UrgentPageDiscardingPolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph_->RemoveSystemNodeObserver(this);
  graph_ = nullptr;
}

void UrgentPageDiscardingPolicy::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel new_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The Memory Pressure Monitor will send notifications at regular interval,
  // |handling_memory_pressure_notification_| prevents this class from trying to
  // reply to multiple notifications at the same time.
  if (handling_memory_pressure_notification_ ||
      new_level != base::MemoryPressureListener::MemoryPressureLevel::
                       MEMORY_PRESSURE_LEVEL_CRITICAL) {
    return;
  }

  handling_memory_pressure_notification_ = true;

  PageDiscardingHelper::GetFromGraph(graph_)->UrgentlyDiscardAPage(
      features::UrgentDiscardingParams::GetParams().discard_strategy(),
      base::BindOnce(
          [](UrgentPageDiscardingPolicy* policy, bool success_unused) {
            DCHECK(policy->handling_memory_pressure_notification_);
            policy->handling_memory_pressure_notification_ = false;
          },
          // |PageDiscardingHelper| and this class are both GraphOwned objects,
          // their lifetime is tied to the Graph's lifetime and both objects
          // will be released sequentially while it's being torn down. This
          // ensures that the reply callback passed to |UrgentlyDiscardAPage|
          // won't ever run after the destruction of this class and so it's safe
          // to use Unretained.
          base::Unretained(this)));
}

}  // namespace policies
}  // namespace performance_manager

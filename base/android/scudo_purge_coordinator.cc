// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scudo_purge_coordinator.h"

#include <malloc.h>

#include <string_view>
#include <utility>

#include "base/android/scudo_features.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace base::android {

// static
std::unique_ptr<ScudoPurgeCoordinator>
ScudoPurgeCoordinator::CreateIfEnabled() {
  if (!base::FeatureList::IsEnabled(base::features::kPeriodicScudoPurge)) {
    return nullptr;
  }
  return std::make_unique<ScudoPurgeCoordinator>(Configuration::FromFeatures());
}

// static
ScudoPurgeCoordinator::Configuration
ScudoPurgeCoordinator::Configuration::FromFeatures() {
  Configuration config;
  config.initial_delay = base::features::kScudoInitialPurgeDelay.Get();
  config.periodic_interval = base::features::kScudoPeriodicPurgeInterval.Get();
  config.background_delay = base::features::kScudoBackgroundPurgeDelay.Get();
  config.enable_foreground_periodic =
      base::features::kScudoEnableForegroundPeriodic.Get();
  config.enable_background_purge =
      base::features::kScudoEnableBackgroundPurge.Get();
  return config;
}

// static
bool ScudoPurgeCoordinator::DefaultMallopt(int param, int value) {
  if (__builtin_available(android 26, *)) {
    return mallopt(param, value) != 0;
  }
  return false;
}

ScudoPurgeCoordinator::ScudoPurgeCoordinator(Configuration config)
    : config_(std::move(config)),
      mallopt_fn_(config_.mallopt_fn_for_testing
                      ? config_.mallopt_fn_for_testing
                      : base::BindRepeating(&DefaultMallopt)) {}

ScudoPurgeCoordinator::~ScudoPurgeCoordinator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  initial_delay_timer_.Stop();
  periodic_timer_.Stop();
  background_timer_.Stop();
}

void ScudoPurgeCoordinator::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_started_);
  is_started_ = true;
  if (config_.enable_foreground_periodic) {
    initial_delay_timer_.Start(
        FROM_HERE, config_.initial_delay,
        base::BindOnce(&ScudoPurgeCoordinator::OnInitialDelayExpired,
                       weak_factory_.GetWeakPtr()));
  }
}

void ScudoPurgeCoordinator::OnForegrounded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_started_ || in_foreground_) {
    return;
  }
  in_foreground_ = true;
  background_timer_.Stop();
  if (config_.enable_foreground_periodic) {
    initial_delay_timer_.Start(
        FROM_HERE, config_.initial_delay,
        base::BindOnce(&ScudoPurgeCoordinator::OnInitialDelayExpired,
                       weak_factory_.GetWeakPtr()));
  }
}

void ScudoPurgeCoordinator::OnBackgrounded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_started_ || !in_foreground_) {
    return;
  }
  in_foreground_ = false;
  initial_delay_timer_.Stop();
  periodic_timer_.Stop();
  if (config_.enable_background_purge) {
    background_timer_.Start(
        FROM_HERE, config_.background_delay,
        base::IgnoreArgs<base::MemoryReductionTaskContext>(
            base::BindOnce(&ScudoPurgeCoordinator::RunBackgroundPurge,
                           weak_factory_.GetWeakPtr())));
  }
}

void ScudoPurgeCoordinator::RunPeriodicForegroundPurge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DispatchPurgeTask(kScudoPurge);
}

void ScudoPurgeCoordinator::RunBackgroundPurge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (in_foreground_) {
    return;
  }
  DispatchPurgeTask(kScudoPurgeAll);
}

void ScudoPurgeCoordinator::DispatchPurgeTask(int purge_param) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ScudoPurgeCoordinator::PerformPurgeInternal, mallopt_fn_,
                     purge_param));
}

// static
void ScudoPurgeCoordinator::PerformPurgeInternal(MalloptFn fn, int param) {
  bool res = fn.Run(param, /*value=*/0);
  if (!res && param == kScudoPurgeAll) {
    fn.Run(kScudoPurge, /*value=*/0);
  }
}

void ScudoPurgeCoordinator::OnInitialDelayExpired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!in_foreground_) {
    return;
  }
  RunPeriodicForegroundPurge();
  if (config_.enable_foreground_periodic) {
    periodic_timer_.Start(
        FROM_HERE, config_.periodic_interval,
        base::BindRepeating(&ScudoPurgeCoordinator::RunPeriodicForegroundPurge,
                            weak_factory_.GetWeakPtr()));
  }
}

}  // namespace base::android

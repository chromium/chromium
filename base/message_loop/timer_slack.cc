// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/timer_slack.h"

#include <atomic>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace base {
namespace features {

constexpr base::Feature kLudicrousTimerSlack{"LudicrousTimerSlack",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

namespace {

constexpr base::FeatureParam<base::TimeDelta> kSlackValueMs{
    &kLudicrousTimerSlack, "slack_ms",
    // 1.5 seconds default slack for this ludicrous experiment.
    base::TimeDelta::FromMilliseconds(1500)};

}  // namespace
}  // namespace features

namespace {

std::atomic<size_t> g_ludicrous_timer_suspend_count{0};

}  // namespace

bool IsLudicrousTimerSlackEnabled() {
  return base::FeatureList::IsEnabled(base::features::kLudicrousTimerSlack);
}

base::TimeDelta GetLudicrousTimerSlack() {
  return features::kSlackValueMs.Get();
}

void SuspendLudicrousTimerSlack() {
  ++g_ludicrous_timer_suspend_count;
}

void ResumeLudicrousTimerSlack() {
  size_t old_count = g_ludicrous_timer_suspend_count.fetch_sub(1);
  DCHECK_LT(0u, old_count);
}

bool IsLudicrousTimerSlackSuspended() {
  return g_ludicrous_timer_suspend_count.load() > 0u;
}

}  // namespace base
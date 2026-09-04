// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_wakeup_counter.h"

#include <atomic>
#include <string>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"

namespace base {

BASE_FEATURE(kInhibitMessagePumpWakeupCounter, FEATURE_DISABLED_BY_DEFAULT);

namespace {

std::atomic_bool g_inhibit_wakeup_counter = false;

}  // namespace

// static
void MessagePumpWakeupCounter::InitializeFeatures() {
  g_inhibit_wakeup_counter.store(
      FeatureList::IsEnabled(kInhibitMessagePumpWakeupCounter),
      std::memory_order_relaxed);
}

// static
void MessagePumpWakeupCounter::InitializeForCurrentThread(
    std::string_view thread_name) {
  MessagePumpWakeupCounter& counter = GetForCurrentThread();
  CHECK_EQ(counter.histogram_, nullptr);
  std::string metric_name =
      base::StrCat({"Scheduling.MessagePump.WakeupCount2.", thread_name});
  counter.histogram_ = BooleanHistogram::FactoryGet(
      metric_name, HistogramBase::kUmaTargetedHistogramFlag);
}

// static
MessagePumpWakeupCounter& MessagePumpWakeupCounter::GetForCurrentThread() {
  constinit static thread_local MessagePumpWakeupCounter counter;
  return counter;
}

void MessagePumpWakeupCounter::RecordWakeup() {
  if (g_inhibit_wakeup_counter.load(std::memory_order_relaxed)) {
    return;
  }
  if (histogram_) {
    // Not subsampling. The average overhead of increasing a bucket count is
    // similar to subsampling itself. In a rare coincidence the bucket could
    // live on a cache line updated from another thread at high frequency,
    // slowing down histogram recording. Assuming it does not happen.
    histogram_->AddBoolean(true);
  }
}

}  // namespace base

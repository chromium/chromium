// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_wakeup_counter.h"

#include <string>

#include "base/check_op.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"

namespace base {

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
  if (histogram_) {
    // Not subsampling. The average overhead of increasing a bucket count is
    // similar to subsampling itself. In a rare coincidence the bucket could
    // live on a cache line updated from another thread at high frequency,
    // slowing down histogram recording. Assuming it does not happen.
    histogram_->AddBoolean(true);
  }
}

}  // namespace base

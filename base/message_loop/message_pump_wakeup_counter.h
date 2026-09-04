// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_WAKEUP_COUNTER_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_WAKEUP_COUNTER_H_

#include <string_view>

#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/metrics/histogram_base.h"

namespace base {

BASE_EXPORT BASE_DECLARE_FEATURE(kInhibitMessagePumpWakeupCounter);

// Records per-thread message pump wakeup counts.
class BASE_EXPORT MessagePumpWakeupCounter {
 public:
  MessagePumpWakeupCounter(const MessagePumpWakeupCounter&) = delete;
  MessagePumpWakeupCounter& operator=(const MessagePumpWakeupCounter&) = delete;

  // Initializes features for this class. See `base::features::Init()`.
  static void InitializeFeatures();

  // Enables metric recording on the current thread.
  static void InitializeForCurrentThread(std::string_view thread_name);
  static MessagePumpWakeupCounter& GetForCurrentThread();

  // Increases the count of wakeups after InitializeForCurrentThread(). No-op
  // otherwise.
  void RecordWakeup();

  void ResetForTesting() { histogram_ = nullptr; }

 private:
  constexpr MessagePumpWakeupCounter() = default;
  ~MessagePumpWakeupCounter() = default;

  // RAW_PTR_EXCLUSION because the class needs to be trivially destructible for
  // thread_local.
  RAW_PTR_EXCLUSION HistogramBase* histogram_ = nullptr;
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_WAKEUP_COUNTER_H_

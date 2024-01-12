// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_ARC_VALUE_EVENT_TRIMMER_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_ARC_VALUE_EVENT_TRIMMER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/tracing/arc_value_event.h"

namespace arc {

// Helper that prevents adding events with the same value.
// For example event values (value:timestamp)
// 1:100 2:101 2:102 2:103 2:104 1:105 2:106
// would be trimmed to
// 1:100 2:101 2:104 1:105 2:106.
class ArcValueEventTrimmer {
 public:
  ArcValueEventTrimmer(ValueEvents* events, ArcValueEvent::Type type);

  ArcValueEventTrimmer(const ArcValueEventTrimmer&) = delete;
  ArcValueEventTrimmer& operator=(const ArcValueEventTrimmer&) = delete;

  ~ArcValueEventTrimmer();

  // May be add the next event, in case it is not trimmed out.
  void MaybeAdd(int64_t timestamp, int value);

  // Resets values it they represent the constant |value|.
  void ResetIfConstant(int value);

 private:
  // In case value has changed, insert last trimmed value.
  void Flush();

  const raw_ptr<ValueEvents> events_;
  const ArcValueEvent::Type type_;
  // Indicate if this is first event that would never be trimmed.
  bool first_event_ = true;
  // Set to true in case last event was trimmed.
  bool was_trimmed_ = false;
  // Timestamp of the last trimmed event.
  int64_t last_trimmed_timestamp_;
  // Value of the last event.
  int last_value_ = 0;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_ARC_VALUE_EVENT_TRIMMER_H_

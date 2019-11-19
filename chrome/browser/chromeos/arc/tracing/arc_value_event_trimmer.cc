// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_value_event_trimmer.h"

namespace arc {

ArcValueEventTrimmer::ArcValueEventTrimmer(ValueEvents* events,
                                           ArcValueEvent::Type type)
    : events_(events), type_(type) {}

ArcValueEventTrimmer::~ArcValueEventTrimmer() {
  Flush();
}

void ArcValueEventTrimmer::MaybeAdd(int64_t timestamp, int value) {
  if (!first_event_ && last_value_ == value) {
    last_trimmed_timestamp_ = timestamp;
    was_trimmed_ = true;
    return;
  }
  Flush();
  events_->emplace_back(timestamp, type_, value);
  last_value_ = value;
  first_event_ = false;
  was_trimmed_ = false;
}

void ArcValueEventTrimmer::ArcValueEventTrimmer::ResetIfConstant(int value) {
  if (events_->size() != 1 || (*events_)[0].value != value)
    return;

  was_trimmed_ = false;
  events_->clear();
}

void ArcValueEventTrimmer::Flush() {
  if (was_trimmed_)
    events_->emplace_back(last_trimmed_timestamp_, type_, last_value_);
  was_trimmed_ = false;
}

}  // namespace arc

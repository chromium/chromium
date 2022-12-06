// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/cros_events_processor.h"

namespace metrics::structured::cros_event {

CrOSEventsProcessor::~CrOSEventsProcessor() = default;

// static
void CrOSEventsProcessor::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {}

// static
void CrOSEventsProcessor::RegisterUserProfilePrefs(
    PrefRegistrySimple* registry) {}

bool CrOSEventsProcessor::ShouldProcessOnEventRecord(const Event& event) {
  return event.IsEventSequenceType();
}

void CrOSEventsProcessor::OnEventsRecord(Event* event) {}

}  // namespace metrics::structured::cros_event

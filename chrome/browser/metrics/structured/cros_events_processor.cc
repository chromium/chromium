// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/cros_events_processor.h"

#include "base/files/file_util.h"
#include "base/logging.h"

namespace metrics::structured::cros_event {

// The path to where the reset counter is stored by platform2.
// This path must remain the same as |kResetCounterPath| in
// platform2/metrics/structured/reset_counter_updater.cc
const char* kResetCounterPath = "/var/lib/metrics/structured/reset-counter";

CrOSEventsProcessor::CrOSEventsProcessor(const char* reset_counter_path) {
  std::string content;
  if (base::ReadFileToString(base::FilePath(reset_counter_path), &content)) {
    std::stringstream ss(content);
    ss >> current_reset_counter_;
  } else {
    LOG(ERROR) << "Failed to read reset_counter file: " << reset_counter_path;
  }
}
CrOSEventsProcessor::~CrOSEventsProcessor() = default;

// static
void CrOSEventsProcessor::RegisterLocalStatePrefs(PrefRegistrySimple*) {}

bool CrOSEventsProcessor::ShouldProcessOnEventRecord(const Event& event) {
  return event.IsEventSequenceType();
}

void CrOSEventsProcessor::OnEventsRecord(Event* event) {
  event->SetEventSequenceMetadata(
      Event::EventSequenceMetadata(current_reset_counter_));
}

void CrOSEventsProcessor::OnEventRecorded(StructuredEventProto* event) {}

void CrOSEventsProcessor::OnProvideIndependentMetrics(
    ChromeUserMetricsExtension* uma_proto) {
  // no-op.
}

}  // namespace metrics::structured::cros_event

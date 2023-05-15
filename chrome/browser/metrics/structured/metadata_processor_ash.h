// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_METADATA_PROCESSOR_ASH_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_METADATA_PROCESSOR_ASH_H_

#include "components/metrics/structured/events_processor_interface.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics::structured {
namespace {

using ::metrics::ChromeUserMetricsExtension;

}

// Retrieves metadata for Ash Chrome and attaches it to the Structured metrics
// payload.
class MetadataProcessorAsh final : public EventsProcessorInterface {
 public:
  // EventsProcessorInterface:
  bool ShouldProcessOnEventRecord(const Event& event) override;
  void OnEventsRecord(Event* event) override;
  void OnProvideIndependentMetrics(
      ChromeUserMetricsExtension* uma_proto) override;

 private:
  // Returns whether the device is enrolled.
  bool IsDeviceEnrolled();
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_METADATA_PROCESSOR_ASH_H_

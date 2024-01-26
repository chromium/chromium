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
using ::metrics::StructuredDataProto;
}

// Retrieves metadata for Ash Chrome and attaches it to the Structured metrics
// payload.
class MetadataProcessorAsh final : public EventsProcessorInterface {
 public:
  // EventsProcessorInterface:
  bool ShouldProcessOnEventRecord(const Event& event) override;
  void OnEventsRecord(Event* event) override;
  void OnEventRecorded(StructuredEventProto* event) override;
  void OnProvideIndependentMetrics(
      ChromeUserMetricsExtension* uma_proto) override;

 private:
  // Helper function to retrieve the primary user and device segments. These
  // functions rely on the
  // chrome/browser/metrics/usertype_by_devicetype_metrics_provider.h for
  // determining the segments.
  StructuredDataProto::DeviceSegment GetDeviceSegment();
  StructuredEventProto::PrimaryUserSegment GetPrimaryUserSegment();

  // Cache of the primary user and device segment.
  std::optional<StructuredDataProto::DeviceSegment> device_segment_;
  std::optional<StructuredEventProto::PrimaryUserSegment> primary_user_segment_;
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_METADATA_PROCESSOR_ASH_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_STRUCTURED_METRICS_TEST_UTIL_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_STRUCTURED_METRICS_TEST_UTIL_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "components/metrics/structured/buildflags/buildflags.h"

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace glic {

// Captures the structured metrics events of the Glic project recorded while
// in scope.
class ScopedStructuredEventCapture
    : public metrics::structured::StructuredMetricsClient::RecordingDelegate {
 public:
  ScopedStructuredEventCapture();
  ~ScopedStructuredEventCapture() override;

  // StructuredMetricsClient::RecordingDelegate:
  bool IsReadyToRecord() const override;
  void RecordEvent(metrics::structured::Event&& event) override;

  std::vector<const metrics::structured::Event*> GetEvents(
      std::string_view event_name) const;

 private:
  std::vector<metrics::structured::Event> events_;
};

// Returns the value of an "int" or enum metric of a captured event, or
// nullopt if the event does not have it. structured.xml "int" metrics are
// encoded as kLong, i.e. a stringified int64, and enums as int.
std::optional<int64_t> GetIntMetric(const metrics::structured::Event& event,
                                    std::string_view metric_name);

}  // namespace glic

#endif  // BUILDFLAG(STRUCTURED_METRICS_ENABLED)

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_STRUCTURED_METRICS_TEST_UTIL_H_

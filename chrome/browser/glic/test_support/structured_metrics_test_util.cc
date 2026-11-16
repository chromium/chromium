// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/structured_metrics_test_util.h"

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace glic {

ScopedStructuredEventCapture::ScopedStructuredEventCapture() {
  metrics::structured::StructuredMetricsClient::Get()->SetDelegate(this);
}

ScopedStructuredEventCapture::~ScopedStructuredEventCapture() {
  metrics::structured::StructuredMetricsClient::Get()->UnsetDelegate();
}

bool ScopedStructuredEventCapture::IsReadyToRecord() const {
  return true;
}

void ScopedStructuredEventCapture::RecordEvent(
    metrics::structured::Event&& event) {
  events_.push_back(std::move(event));
}

std::vector<const metrics::structured::Event*>
ScopedStructuredEventCapture::GetEvents(std::string_view event_name) const {
  std::vector<const metrics::structured::Event*> result;
  for (const metrics::structured::Event& event : events_) {
    if (event.project_name() == "Glic" && event.event_name() == event_name) {
      result.push_back(&event);
    }
  }
  return result;
}

std::optional<int64_t> GetIntMetric(const metrics::structured::Event& event,
                                    std::string_view metric_name) {
  auto it = event.metric_values().find(std::string(metric_name));
  if (it == event.metric_values().end()) {
    return std::nullopt;
  }
  const base::Value& value = it->second.value;
  if (value.is_int()) {
    return value.GetInt();
  }
  int64_t result;
  if (value.is_string() && base::StringToInt64(value.GetString(), &result)) {
    return result;
  }
  return std::nullopt;
}

}  // namespace glic

#endif  // BUILDFLAG(STRUCTURED_METRICS_ENABLED)

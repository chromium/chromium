// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/metrics/structured_metrics_service_wrapper.h"

#include "base/system/sys_info.h"
#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

StructuredMetricsServiceWrapper::StructuredMetricsServiceWrapper() = default;
StructuredMetricsServiceWrapper::~StructuredMetricsServiceWrapper() = default;

void StructuredMetricsServiceWrapper::BindReceiver(
    mojo::PendingReceiver<::crosapi::mojom::StructuredMetricsService>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

// static
base::TimeDelta StructuredMetricsServiceWrapper::ComputeEventUptime(
    base::TimeDelta system_uptime,
    base::TimeDelta system_timestamp,
    base::TimeDelta event_timestamp) {
  return system_uptime -
         std::max(system_timestamp - event_timestamp, base::Seconds(0));
}

void StructuredMetricsServiceWrapper::Record(
    std::vector<::metrics::structured::Event> events) {
  for (auto& event : events) {
    if (event.IsEventSequenceType()) {
      event.SetRecordedTimeSinceBoot(ComputeEventUptime(
          base::SysInfo::Uptime(),
          base::Time::NowFromSystemTime() - base::Time::UnixEpoch(),
          // if event does not have system uptime field populated, set it as the
          // current system timestamp.
          event.recorded_time_since_boot()));
    }
    ::metrics::structured::StructuredMetricsClient::Get()->Record(
        std::move(event));
  }
}

}  // namespace ash

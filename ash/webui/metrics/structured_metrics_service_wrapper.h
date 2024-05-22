// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_METRICS_STRUCTURED_METRICS_SERVICE_WRAPPER_H_
#define ASH_WEBUI_METRICS_STRUCTURED_METRICS_SERVICE_WRAPPER_H_

#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "components/metrics/structured/event.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

// Implements the StructuredMetricsService mojo interface to record events.
// Wrapper to validate and record structured metrics received from WebUI. Lives
// on the UI thread.
class StructuredMetricsServiceWrapper final
    : public ::crosapi::mojom::StructuredMetricsService {
 public:
  StructuredMetricsServiceWrapper();
  StructuredMetricsServiceWrapper(const StructuredMetricsServiceWrapper&) =
      delete;
  ~StructuredMetricsServiceWrapper() override;

  void BindReceiver(
      mojo::PendingReceiver<::crosapi::mojom::StructuredMetricsService>
          receiver);

  // Compute the Structured Event's uptime based on its timestamp and the
  // system uptime and timestamp when the event is received by
  // StructuredMetricsServiceWrapper.
  // Static for unit testing.
  static base::TimeDelta ComputeEventUptime(base::TimeDelta system_uptime,
                                            base::TimeDelta system_timestamp,
                                            base::TimeDelta event_timestamp);

  // crosapi::mojom::StructuredMetricsService
  void Record(std::vector<::metrics::structured::Event> events) override;

 private:
  mojo::ReceiverSet<::crosapi::mojom::StructuredMetricsService> receivers_;
};

}  // namespace ash

#endif  // ASH_WEBUI_METRICS_STRUCTURED_METRICS_SERVICE_WRAPPER_H_

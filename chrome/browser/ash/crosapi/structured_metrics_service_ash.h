// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_STRUCTURED_METRICS_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_STRUCTURED_METRICS_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "components/metrics/structured/event.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the StructuredMetricsService mojo interface to record events.
// Wrapper to validate and record structured metrics received from lacros. Lives
// on the UI thread.
class StructuredMetricsServiceAsh final
    : public mojom::StructuredMetricsService {
 public:
  StructuredMetricsServiceAsh();
  StructuredMetricsServiceAsh(const StructuredMetricsServiceAsh&) = delete;
  StructuredMetricsServiceAsh& operator=(const StructuredMetricsServiceAsh&) =
      delete;
  ~StructuredMetricsServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::StructuredMetricsService> receiver);

  // crosapi::mojom::StructuredMetricsService
  void Record(std::vector<::metrics::structured::Event> events) override;

 private:
  mojo::ReceiverSet<mojom::StructuredMetricsService> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_STRUCTURED_METRICS_SERVICE_ASH_H_

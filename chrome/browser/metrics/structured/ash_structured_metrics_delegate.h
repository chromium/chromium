// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_ASH_STRUCTURED_METRICS_DELEGATE_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_ASH_STRUCTURED_METRICS_DELEGATE_H_

#include "chrome/browser/metrics/structured/structured_metrics_key_events_observer.h"
#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace metrics::structured {

// Recording delegate for Ash Chrome.
//
// Although the main service also lives in Ash, this recorder sends events using
// a mojo pipe. Instantiating a mojo pipe adds little overhead and provides lots
// of benefits out of the box (ie message buffer).
class AshStructuredMetricsDelegate
    : public StructuredMetricsClient::RecordingDelegate {
 public:
  AshStructuredMetricsDelegate();
  AshStructuredMetricsDelegate(const AshStructuredMetricsDelegate& recorder) =
      delete;
  AshStructuredMetricsDelegate& operator=(
      const AshStructuredMetricsDelegate& recorder) = delete;
  ~AshStructuredMetricsDelegate() override;

  // Sets up the recorder. This should be called after CrosApi is initialized,
  // which is done in PreProfileInit() of the browser process setup.
  void Initialize();

  // RecordingDelegate:
  void RecordEvent(Event&& event) override;
  bool IsReadyToRecord() const override;

 private:
  mojo::Remote<crosapi::mojom::StructuredMetricsService> remote_;
  std::unique_ptr<StructuredMetricsKeyEventsObserver> key_events_observer_;
  bool is_initialized_ = false;
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_ASH_STRUCTURED_METRICS_DELEGATE_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_METRICS_REPORTING_OBSERVER_H_
#define CHROME_BROWSER_LACROS_METRICS_REPORTING_OBSERVER_H_

#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

// Observes ash-chrome for changes in metrics reporting consent state. The UX
// goal is to have a single "shared" metrics reporting state across the OS and
// browser. Ash owns the canonical state, so lacros observes it for changes.
class MetricsReportingObserver
    : public crosapi::mojom::MetricsReportingObserver {
 public:
  // |local_state| is the "Local State" (non-profile) preferences store.
  explicit MetricsReportingObserver(PrefService* local_state);
  MetricsReportingObserver(const MetricsReportingObserver&) = delete;
  MetricsReportingObserver& operator=(const MetricsReportingObserver&) = delete;
  ~MetricsReportingObserver() override;

  void Init();

  // crosapi::mojom::MetricsObserver:
  void OnMetricsReportingChanged(bool enabled) override;

 private:
  friend class TestMetricsReportingObserver;

  // Updates the metrics reporting if it has changed from the previous state.
  void UpdateMetricsReportingState(bool enabled);

  // Changes the metrics reporting state. Virtual for testing.
  virtual void DoChangeMetricsReportingState(bool enabled);

  // Returns whether metrics reporting is enabled.
  bool IsMetricsReportingEnabled() const;

  // Local state (non-profile) preferences.
  PrefService* const local_state_;

  // Mojo connection to ash.
  mojo::Remote<crosapi::mojom::MetricsReporting> metrics_reporting_remote_;

  // Receives mojo messages from ash.
  mojo::Receiver<crosapi::mojom::MetricsReportingObserver> receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_METRICS_REPORTING_OBSERVER_H_

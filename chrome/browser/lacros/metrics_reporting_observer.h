// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_METRICS_REPORTING_OBSERVER_H_
#define CHROME_BROWSER_LACROS_METRICS_REPORTING_OBSERVER_H_

#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// Observes ash-chrome for changes in metrics reporting consent state. The UX
// goal is to have a single "shared" metrics reporting state across the OS and
// browser. Ash owns the canonical state, so lacros observes it for changes.
class MetricsReportingObserver
    : public crosapi::mojom::MetricsReportingObserver {
 public:
  // Allows stubbing the real metrics service in test.
  class MetricsServiceProxy {
   public:
    virtual ~MetricsServiceProxy() = default;

    virtual void SetReportingEnabled(bool enabled) = 0;
    virtual void SetExternalClientId(const std::string& id) = 0;
    // Recreates the client id iff metrics reporting is enabled.
    virtual void RecreateClientIdIfNecessary() = 0;
  };

  static std::unique_ptr<MetricsReportingObserver> CreateObserver(
      metrics::MetricsService* metrics_service);

  explicit MetricsReportingObserver(
      std::unique_ptr<MetricsServiceProxy> metrics_service);

  MetricsReportingObserver(const MetricsReportingObserver&) = delete;
  MetricsReportingObserver& operator=(const MetricsReportingObserver&) = delete;
  ~MetricsReportingObserver() override;

  // Loads the initial metrics settings from ash.
  static void InitSettingsFromAsh();

  // crosapi::mojom::MetricsObserver:
  void OnMetricsReportingChanged(
      bool enabled,
      const std::optional<std::string>& client_id) override;

 private:
  friend class TestMetricsReportingObserver;

  std::unique_ptr<MetricsServiceProxy> metrics_service_;

  // Mojo connection to ash.
  mojo::Remote<crosapi::mojom::MetricsReporting> metrics_reporting_remote_;

  // Receives mojo messages from ash.
  mojo::Receiver<crosapi::mojom::MetricsReportingObserver> receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_METRICS_REPORTING_OBSERVER_H_

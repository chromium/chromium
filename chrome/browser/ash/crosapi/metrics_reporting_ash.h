// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_METRICS_REPORTING_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_METRICS_REPORTING_ASH_H_

#include <memory>

#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefService;

namespace crosapi {

// The ash-chrome implementation of the MetricsReporting crosapi interface.
// This class must only be used from the main thread.
class MetricsReportingAsh : public mojom::MetricsReporting {
 public:
  // Allows stubbing out the metrics reporting subsystem.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void SetMetricsReportingEnabled(bool enabled) = 0;
  };
  // Constructor for production. Uses the real metrics reporting subsystem.
  explicit MetricsReportingAsh(PrefService* local_state);
  // Constructor for testing.
  MetricsReportingAsh(std::unique_ptr<Delegate> delegate,
                      PrefService* local_state);
  MetricsReportingAsh(const MetricsReportingAsh&) = delete;
  MetricsReportingAsh& operator=(const MetricsReportingAsh&) = delete;
  ~MetricsReportingAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::MetricsReporting> receiver);

  // crosapi::mojom::MetricsReporting:
  void AddObserver(
      mojo::PendingRemote<mojom::MetricsReportingObserver> observer) override;
  void SetMetricsReportingEnabled(
      bool enabled,
      SetMetricsReportingEnabledCallback callback) override;

 private:
  // Notifies all observers of the current metrics state.
  void NotifyObservers();

  // Returns whether metrics reporting is enabled.
  bool IsMetricsReportingEnabled() const;

  std::unique_ptr<Delegate> delegate_;

  // In production, owned by g_browser_process, which outlives this object.
  PrefService* const local_state_;

  // Observes the metrics enabled pref.
  PrefChangeRegistrar pref_change_registrar_;

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::MetricsReporting> receivers_;

  // This class supports any number of observers.
  mojo::RemoteSet<mojom::MetricsReportingObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_METRICS_REPORTING_ASH_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_METRICS_REPORTING_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_METRICS_REPORTING_ASH_H_

#include <memory>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace metrics {
class MetricsService;
}  // namespace metrics

namespace crosapi {

// The ash-chrome implementation of the MetricsReporting crosapi interface.
// This class must only be used from the main thread.
class MetricsReportingAsh : public mojom::MetricsReporting {
 public:
  // Allows stubbing out the metrics reporting subsystem.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual bool IsMetricsReportingEnabled() = 0;
    virtual void SetMetricsReportingEnabled(bool enabled) = 0;
    virtual std::string GetClientId() = 0;
    virtual base::CallbackListSubscription AddEnablementObserver(
        const base::RepeatingCallback<void(bool)>& observer) = 0;
  };
  static std::unique_ptr<MetricsReportingAsh> CreateMetricsReportingAsh(
      metrics::MetricsService* metrics_service);

  // Constructs a metrics service impl. Do not use this directly and use the
  // Factory interface instead.
  explicit MetricsReportingAsh(std::unique_ptr<Delegate> delegate);
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

  void OnEnablementChange(bool enabled);

 private:
  // Notifies all observers of the current metrics state.
  void NotifyObservers();

  std::unique_ptr<Delegate> delegate_;

  // Handle for the observer watching the metrics enablement state. This needs
  // to remain active for the lifetime of the observer.
  base::CallbackListSubscription observer_subscription_;

  // Observes the metrics enabled pref.
  PrefChangeRegistrar pref_change_registrar_;

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::MetricsReporting> receivers_;

  // This class supports any number of observers.
  mojo::RemoteSet<mojom::MetricsReportingObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_METRICS_REPORTING_ASH_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_MOJO_SERVICE_EVENTS_OBSERVER_BASE_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_MOJO_SERVICE_EVENTS_OBSERVER_BASE_H_

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace reporting {

// A base class containing common functionalities needed for observing and
// reporting mojo service events.
template <typename Interface>
class MojoServiceEventsObserverBase : public MetricEventObserver {
 public:
  explicit MojoServiceEventsObserverBase(Interface* interface)
      : receiver_{interface} {}

  MojoServiceEventsObserverBase(const MojoServiceEventsObserverBase&) = delete;
  MojoServiceEventsObserverBase& operator=(
      const MojoServiceEventsObserverBase&) = delete;

  ~MojoServiceEventsObserverBase() override = default;

  void SetReportingEnabled(bool is_enabled) override {
    is_reporting_enabled_ = is_enabled;
    SetObservation();
  }

  void SetOnEventObservedCallback(MetricRepeatingCallback cb) override {
    CHECK(!on_event_observed_cb_);
    on_event_observed_cb_ = std::move(cb);
  }

 protected:
  virtual void AddObserver() = 0;

  void OnEventObserved(MetricData metric_data) {
    if (!on_event_observed_cb_) {
      DVLOG(1) << "Event observed but callback is not set.";
      return;
    }
    on_event_observed_cb_.Run(std::move(metric_data));
  }

  mojo::PendingRemote<Interface> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  mojo::Receiver<Interface> receiver_;

 private:
  void SetObservation() {
    if (receiver_.is_bound()) {
      receiver_.reset();
    }

    if (is_reporting_enabled_) {
      AddObserver();
      receiver_.set_disconnect_handler(
          base::BindOnce(&MojoServiceEventsObserverBase::SetObservation,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  bool is_reporting_enabled_ = false;

  MetricRepeatingCallback on_event_observed_cb_;

  base::WeakPtrFactory<MojoServiceEventsObserverBase> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_MOJO_SERVICE_EVENTS_OBSERVER_BASE_H_

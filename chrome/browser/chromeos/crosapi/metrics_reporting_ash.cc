// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/metrics_reporting_ash.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"

namespace crosapi {

MetricsReportingAsh::MetricsReportingAsh(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
  pref_change_registrar_.Init(local_state_);
  // base::Unretained() is safe because PrefChangeRegistrar removes all
  // observers when it is destroyed.
  pref_change_registrar_.Add(metrics::prefs::kMetricsReportingEnabled,
                             base::Bind(&MetricsReportingAsh::NotifyObservers,
                                        base::Unretained(this)));
}

MetricsReportingAsh::~MetricsReportingAsh() = default;

void MetricsReportingAsh::BindReceiver(
    mojo::PendingReceiver<mojom::MetricsReporting> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MetricsReportingAsh::AddObserver(
    mojo::PendingRemote<mojom::MetricsReportingObserver> observer) {
  mojo::Remote<mojom::MetricsReportingObserver> remote(std::move(observer));
  // Fire the observer with the initial value.
  bool enabled = IsMetricsReportingEnabled();
  remote->OnMetricsReportingChanged(enabled);
  // Store the observer for future notifications.
  observers_.Add(std::move(remote));
}

void MetricsReportingAsh::SetMetricsReportingEnabled(
    bool enabled,
    SetMetricsReportingEnabledCallback callback) {
  // TODO(https://crbug.com/1148604): Implement this.
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void MetricsReportingAsh::NotifyObservers() {
  bool enabled = IsMetricsReportingEnabled();
  for (auto& observer : observers_) {
    observer->OnMetricsReportingChanged(enabled);
  }
}

bool MetricsReportingAsh::IsMetricsReportingEnabled() const {
  return local_state_->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
}

}  // namespace crosapi

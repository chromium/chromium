// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/metrics_reporting_observer.h"

#include "base/check.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"

MetricsReportingObserver::MetricsReportingObserver(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

MetricsReportingObserver::~MetricsReportingObserver() = default;

void MetricsReportingObserver::Init() {
  auto* lacros_service = chromeos::LacrosChromeServiceImpl::Get();
  if (!lacros_service->IsMetricsReportingAvailable()) {
    LOG(WARNING) << "MetricsReporting API not available";
    return;
  }

  // Set the initial state.
  UpdateMetricsReportingState(
      lacros_service->init_params()->ash_metrics_enabled);

  // Add this object as an observer. The observer will fire with the current
  // state in ash, to avoid races where ash might change state between the
  // initial state above from lacros startup and the observer being added.
  lacros_service->BindMetricsReporting(
      metrics_reporting_remote_.BindNewPipeAndPassReceiver());
  metrics_reporting_remote_->AddObserver(receiver_.BindNewPipeAndPassRemote());
}

void MetricsReportingObserver::OnMetricsReportingChanged(bool enabled) {
  UpdateMetricsReportingState(enabled);
}

void MetricsReportingObserver::UpdateMetricsReportingState(bool enabled) {
  // ChangeMetricsReportingState() unconditionally records its own UMA metrics
  // and clears stability metrics. Only call it if the enabled state actually
  // changed.
  if (enabled == IsMetricsReportingEnabled())
    return;

  DoChangeMetricsReportingState(enabled);
}

void MetricsReportingObserver::DoChangeMetricsReportingState(bool enabled) {
  ChangeMetricsReportingState(enabled);
}

bool MetricsReportingObserver::IsMetricsReportingEnabled() const {
  return local_state_->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
}

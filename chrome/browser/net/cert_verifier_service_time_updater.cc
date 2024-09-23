// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/cert_verifier_service_time_updater.h"

#include "base/time/time.h"
#include "components/network_time/network_time_tracker.h"
#include "components/network_time/time_tracker/time_tracker.h"
#include "content/public/browser/network_service_instance.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

CertVerifierServiceTimeUpdater::CertVerifierServiceTimeUpdater(
    network_time::NetworkTimeTracker* tracker) {
  DCHECK(tracker);
  tracker->AddObserver(this);

  // If the time is already available on construction, do an immediate update.
  network_time::TimeTracker::TimeTrackerState state;
  if (tracker->GetTrackerState(&state)) {
    content::GetCertVerifierServiceFactory()->UpdateNetworkTime(
        state.system_time, state.system_ticks, state.known_time);
  }
}

CertVerifierServiceTimeUpdater::~CertVerifierServiceTimeUpdater() {
  CHECK(!IsInObserverList());
}

void CertVerifierServiceTimeUpdater::OnNetworkTimeChanged(
    network_time::TimeTracker::TimeTrackerState state) {
  content::GetCertVerifierServiceFactory()->UpdateNetworkTime(
      state.system_time, state.system_ticks, state.known_time);
}

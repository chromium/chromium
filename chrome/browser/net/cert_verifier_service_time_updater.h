// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CERT_VERIFIER_SERVICE_TIME_UPDATER_H_
#define CHROME_BROWSER_NET_CERT_VERIFIER_SERVICE_TIME_UPDATER_H_

#include "components/network_time/network_time_tracker.h"
#include "components/network_time/time_tracker/time_tracker.h"

class CertVerifierServiceTimeUpdater
    : public network_time::NetworkTimeTracker::NetworkTimeObserver {
 public:
  // CertVerifierServiceTimeUpdater is expected to outlive |tracker| (and will
  // CHECK if it's destroyed first).
  explicit CertVerifierServiceTimeUpdater(
      network_time::NetworkTimeTracker* tracker);
  ~CertVerifierServiceTimeUpdater() override;

  void OnNetworkTimeChanged(
      network_time::TimeTracker::TimeTrackerState state) override;
};

#endif  // CHROME_BROWSER_NET_CERT_VERIFIER_SERVICE_TIME_UPDATER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_SUSPEND_PERF_REPORTER_H_
#define CHROME_BROWSER_ASH_POWER_SUSPEND_PERF_REPORTER_H_

#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

// SuspendPerfReporter logs performance related UMA metrics 1 min after resuming
// the device.
//
// This takes snapshots of the UMA histograms of the metrics listed in
// suspend_perf_reporter.cc as `kMetricNames` when powerd sends `SuspendDone`
// D-Bus signal. A second set of snapshots is taken 1 minute later, and the
// differences between the snapshots are reported as new metrics. The name of
// new metrics are suffixed with ".1MinAfterResume".
class SuspendPerfReporter : public chromeos::PowerManagerClient::Observer {
 public:
  explicit SuspendPerfReporter(
      chromeos::PowerManagerClient* power_manager_client);

  SuspendPerfReporter(const SuspendPerfReporter&) = delete;
  SuspendPerfReporter& operator=(const SuspendPerfReporter&) = delete;

  ~SuspendPerfReporter() override;

  // PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta duration) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  base::OneShotTimer timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_SUSPEND_PERF_REPORTER_H_

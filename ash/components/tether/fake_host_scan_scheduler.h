// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TETHER_FAKE_HOST_SCAN_SCHEDULER_H_
#define ASH_COMPONENTS_TETHER_FAKE_HOST_SCAN_SCHEDULER_H_

#include "ash/components/tether/host_scan_scheduler.h"

namespace ash {

namespace tether {

// Test double for HostScanScheduler.
class FakeHostScanScheduler : public HostScanScheduler {
 public:
  FakeHostScanScheduler();

  FakeHostScanScheduler(const FakeHostScanScheduler&) = delete;
  FakeHostScanScheduler& operator=(const FakeHostScanScheduler&) = delete;

  ~FakeHostScanScheduler() override;

  int num_attempted_scans() { return num_attempted_scans_; }

  // HostScanScheduler:
  void AttemptScanIfOffline() override;

 private:
  int num_attempted_scans_ = 0;
};

}  // namespace tether

}  // namespace ash

#endif  // ASH_COMPONENTS_TETHER_FAKE_HOST_SCAN_SCHEDULER_H_

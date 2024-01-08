// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SHUTDOWN_MONITOR_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SHUTDOWN_MONITOR_H_

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

class Profile;

namespace borealis {

// Manages the automatic shutdown of borealis either immediately, or via a timed
// delay.
class BorealisShutdownMonitor {
 public:
  explicit BorealisShutdownMonitor(Profile* profile);
  ~BorealisShutdownMonitor();

  // Initiate a delayed shutdown, which will trigger the real shutdown after a
  // fixed time period, unless the shutdown is aborted in the interim. If a
  // delayed shutdown is in progress, this will reset the delay.
  void ShutdownWithDelay();

  // Initiate a shutdown immediately, without the delay.
  void ShutdownNow();

  // Cancels any in-progress delayed shutdowns.
  void CancelDelayedShutdown();

  // Overrides the default delay of the shutdown.
  void SetShutdownDelayForTesting(base::TimeDelta delay);

 private:
  // Called when the shutdown timer finishes.
  void OnShutdownTimerElapsed();

  // The profile which we will shutdown borealis for.
  raw_ptr<Profile> profile_;

  // The length of time we wait before issuing a shutdown after a delayed
  // shutdown is requested.
  base::TimeDelta delay_;

  // The currently in-flight request to shutdown borealis. This will be in the
  // default state unless a request is actually underway.
  base::CancelableOnceClosure in_progress_request_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SHUTDOWN_MONITOR_H_

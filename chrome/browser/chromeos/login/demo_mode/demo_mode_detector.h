// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_MODE_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_MODE_DETECTOR_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/idle_detector.h"

class PrefRegistrySimple;

namespace chromeos {

// Helper for idle state and demo-mode detection.
// Should be initialized on OOBE start.
class DemoModeDetector {
 public:
  DemoModeDetector();
  virtual ~DemoModeDetector();

  void InitDetection();
  void StopDetection();

  // Registers the preference for derelict state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Sets an alternative clock for testing purposes.
  void SetTickClockForTest(const base::TickClock* test_clock);

  static const base::TimeDelta kDerelictDetectionTimeout;
  static const base::TimeDelta kDerelictIdleTimeout;
  static const base::TimeDelta kOobeTimerUpdateInterval;

 private:
  void StartIdleDetection();
  void StartOobeTimer();
  void OnIdle();
  void OnOobeTimerUpdate();
  void SetupTimeouts();
  bool IsDerelict();

  // Total time this machine has spent on OOBE.
  base::TimeDelta time_on_oobe_;

  std::unique_ptr<IdleDetector> idle_detector_;

  base::RepeatingTimer oobe_timer_;

  // Timeout to detect if the machine is in a derelict state.
  base::TimeDelta derelict_detection_timeout_;

  // Timeout before showing our demo app if the machine is in a derelict state.
  base::TimeDelta derelict_idle_timeout_;

  // Time between updating our total time on oobe.
  base::TimeDelta oobe_timer_update_interval_;

  bool demo_launched_ = false;

  const base::TickClock* tick_clock_;

  base::WeakPtrFactory<DemoModeDetector> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DemoModeDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_MODE_DETECTOR_H_

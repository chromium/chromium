// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_POWER_BUTTON_RACE_DETECTOR_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_POWER_BUTTON_RACE_DETECTOR_H_

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {
namespace quick_unlock {

// Detects when we have a finperprint scan and a power button press done
// within the span of 1 second, and logs to a histogram when that happens.
class FingerprintPowerButtonRaceDetector
    : public chromeos::PowerManagerClient::Observer {
 public:
  explicit FingerprintPowerButtonRaceDetector(
      chromeos::PowerManagerClient* power_manager_client);
  ~FingerprintPowerButtonRaceDetector() override;

  void PowerButtonEventReceived(bool down, base::TimeTicks timestamp) override;

  void FingerprintScanReceived(base::TimeTicks timestamp);

 private:
  friend class FingerprintPowerButtonRaceDetectorTest;

  enum class EventType { NONE, FINGERPRINT, POWER_BUTTON };

  void HandleEventHappened(base::TimeTicks timestamp, EventType event_type);

  EventType last_event_type_ = EventType::NONE;
  base::TimeTicks last_event_timestamp_;

  // biod takes some time to process a fingerprint touch (< 0.3 seconds)
  // and we need to consider this delay. 1 second gives enough time to
  // capture both events.
  base::TimeDelta race_time_window_ = base::Seconds(1);

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};

  const std::string power_button_pressed_histogram_name_ =
      "Power.PowerButtonPressed";
  const std::string fingerprint_scan_histogram_name_ =
      "Fingerprint.FingerprintScanDone";
  const std::string fingerprint_power_button_race_histogram_name_ =
      "Fingerprint.FingerprintPowerButtonRace";
};

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_POWER_BUTTON_RACE_DETECTOR_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/fingerprint_power_button_race_detector.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"

#include "base/logging.h"

namespace ash {
namespace quick_unlock {

FingerprintPowerButtonRaceDetector::FingerprintPowerButtonRaceDetector(
    chromeos::PowerManagerClient* power_manager_client) {
  power_manager_client_observation_.Observe(power_manager_client);
}

FingerprintPowerButtonRaceDetector::~FingerprintPowerButtonRaceDetector() =
    default;

void FingerprintPowerButtonRaceDetector::PowerButtonEventReceived(
    bool down,
    base::TimeTicks timestamp) {
  base::UmaHistogramBoolean(power_button_pressed_histogram_name_, true);
  HandleEventHappened(timestamp, EventType::POWER_BUTTON);
}

void FingerprintPowerButtonRaceDetector::FingerprintScanReceived(
    base::TimeTicks timestamp) {
  base::UmaHistogramBoolean(fingerprint_scan_histogram_name_, true);
  HandleEventHappened(timestamp, EventType::FINGERPRINT);
}

void FingerprintPowerButtonRaceDetector::HandleEventHappened(
    base::TimeTicks timestamp,
    EventType event_type) {
  DCHECK_NE(event_type, EventType::NONE);
  if (last_event_type_ != EventType::NONE && event_type != last_event_type_) {
    VLOG(0) << (timestamp - last_event_timestamp_ <= race_time_window_);
    base::UmaHistogramBoolean(
        fingerprint_power_button_race_histogram_name_,
        timestamp - last_event_timestamp_ <= race_time_window_);
  }
  last_event_timestamp_ = timestamp;
  last_event_type_ = event_type;
}

}  // namespace quick_unlock
}  // namespace ash

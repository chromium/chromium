// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_VISION_KIOSK_VISION_H_
#define CHROME_BROWSER_ASH_APP_MODE_VISION_KIOSK_VISION_H_

#include "chrome/browser/ash/app_mode/vision/internal/pref_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash::kiosk_vision {

// Manages the hierarchy of objects involved in the Kiosk Vision ML feature.
//
// Its responsibilities include enabling and disabling the feature based on
// prefs; communicating with the CrOS camera service to retrieve ML model
// detections; and processing and forwarding detections to the backend telemetry
// API and the Kiosk web app.
class KioskVision {
 public:
  explicit KioskVision(PrefService* pref_service);
  KioskVision(const KioskVision&) = delete;
  KioskVision& operator=(const KioskVision&) = delete;
  ~KioskVision() = default;

 private:
  void Enable();
  void Disable();

  PrefObserver pref_observer_;
};

inline constexpr char kKioskVisionDlcId[] = "kiosk-vision";

}  // namespace ash::kiosk_vision

#endif  // CHROME_BROWSER_ASH_APP_MODE_VISION_KIOSK_VISION_H_

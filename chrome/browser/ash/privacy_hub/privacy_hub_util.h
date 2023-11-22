// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_UTIL_H_
#define CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_UTIL_H_

#include <string>

class AppAccessNotifier;

namespace ash {

class PrivacyHubDelegate;

namespace privacy_hub_util {

// Sets a given frontend handler withing the controller.
void SetFrontend(PrivacyHubDelegate* ptr);

// Returns the current switch state of the microphone.
bool MicrophoneSwitchState();

// Returns whether the camera switch should be disabled.
// Note that the UI switch will always be disabled if no camera is connected
// to the device, irrespective of what this function returns.
bool ShouldForceDisableCameraSwitch();

// Needs to be called for the Privacy Hub to be aware of the camera count.
void SetUpCameraCountObserver();

// Notifies the Privacy Hub controller.
void TrackGeolocationAttempted(const std::string& name);

// Notifies the Privacy Hub controller.
void TrackGeolocationRelinquished(const std::string& name);

// Checks if we use the fallback solution for the camera LED
// (go/privacy-hub:camera-led-fallback).
// TODO(b/289510726): remove when all cameras fully support the software
// switch.
bool UsingCameraLEDFallback();

// Used to override the value of the LED Fallback value in tests.
// Should not be nested.
// TODO(b/289510726): remove when all cameras fully support the software
// switch.
class ScopedCameraLedFallbackForTesting {
 public:
  explicit ScopedCameraLedFallbackForTesting(bool value);
  ~ScopedCameraLedFallbackForTesting();
};

// Sets an AppAccessNotifier instance to be used by the privacy hub
void SetAppAccessNotifier(AppAccessNotifier* app_access_notifier);

}  // namespace privacy_hub_util

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_UTIL_H_

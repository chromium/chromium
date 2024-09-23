// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_UTIL_H_
#define CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_UTIL_H_

#include <string>

#include "ash/constants/geolocation_access_level.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_types.mojom.h"

class AppAccessNotifier;

namespace ash {

class PrivacyHubDelegate;

namespace privacy_hub_util {

using ContentType = content_settings::mojom::ContentSettingsType;
using SystemPermissionChangedCallback =
    base::RepeatingCallback<void(ContentType /*type*/, bool /*is_blocked*/)>;

// This class is used to maintain observation of the permissions blocked at the
// system level. The callback provided in the constructor is always called when
// the system permissions change. The observation will continue until the object
// is destroyed.
class ContentBlockObservation {
 public:
  ContentBlockObservation(const ContentBlockObservation&) = delete;
  ContentBlockObservation& operator=(const ContentBlockObservation&) = delete;
  virtual ~ContentBlockObservation() = default;

 protected:
  ContentBlockObservation() = default;
};

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

// Checks if the user can modify the ChromeOS system location toggle in the OOBE
// consents screen. Returns true, if the underlying pref is not managed.
// Returns false if the Privacy Hub Location feature flag is not enabled.
bool IsCrosLocationOobeNegotiationNeeded();

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

// Returns a pair with sunrise and sunset time.
std::pair<base::Time, base::Time> SunriseSunsetSchedule();

// Returns true if the content type is blocked in the OS.
bool ContentBlocked(ContentType type);

// If the call is successful, an observer is created that will always call the
// callback when the set of content types blocked at the OS level
// changes. It will also be called once for each content type immediately to
// establish the initial value. In case the creation is not successful, nullptr
// is returned.
std::unique_ptr<ContentBlockObservation> CreateObservationForBlockedContent(
    SystemPermissionChangedCallback callback);

// Opens the system settings page that allows OS level control for the provided
// content type if such settings page exists.
void OpenSystemSettings(ContentType type);

class ScopedUserPermissionPrefForTest {
 public:
  // The AccessLevel is equivalent to GeolocationAccess level.
  // In the context of ScopedUserPermissionPrefForTest it is used
  // to specify access level for camera and microphone as well, however the
  // kOnlyAllowedForSystem is to be used only for geolocation.
  using AccessLevel = GeolocationAccessLevel;

  // Sets the permission level in the OS
  ScopedUserPermissionPrefForTest(ContentType type, AccessLevel access_level);
  // After destruction the pref is returned to the original state.
  ~ScopedUserPermissionPrefForTest();

 private:
  const ContentType content_type_;
  const AccessLevel previous_access_level_;
};

}  // namespace privacy_hub_util

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_UTIL_H_

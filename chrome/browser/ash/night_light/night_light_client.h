// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NIGHT_LIGHT_NIGHT_LIGHT_CLIENT_H_
#define CHROME_BROWSER_ASH_NIGHT_LIGHT_NIGHT_LIGHT_CLIENT_H_

namespace ash {

// Periodically requests the IP-based geolocation and provides it to the
// NightLightController running in ash.
class NightLightClient {
 public:
  virtual ~NightLightClient();

  // Starts watching changes in the Night Light schedule type in order to begin
  // periodically pushing user's IP-based geoposition to NightLightController as
  // long as the type is set to "sunset to sunrise" or "custom".
  virtual void Start() = 0;

  // This class should respect the system geolocation permission. When the
  // permission is disabled, no requests should be dispatched.
  // Called from `ash::Preferences::ApplyPreferences()`.
  virtual void OnSystemGeolocationPermissionChanged(bool enabled) = 0;

  // Returns global NightLightClient if created or nullptr.
  static NightLightClient* Get();

 protected:
  NightLightClient();

 private:
  // Virtual so that it can be overriden by a fake implementation in unit tests
  // that doesn't request actual geopositions.
  virtual void RequestGeoposition() = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NIGHT_LIGHT_NIGHT_LIGHT_CLIENT_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NIGHT_LIGHT_NIGHT_LIGHT_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_NIGHT_LIGHT_NIGHT_LIGHT_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/night_light_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/night_light/night_light_client.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/ash/components/settings/timezone_settings.h"

namespace base {
class Clock;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}

namespace ash {

// Periodically requests the IP-based geolocation and provides it to the
// NightLightController running in ash.
class NightLightClientImpl : public ash::NightLightClient,
                             public ash::NightLightController::Observer,
                             public ash::system::TimezoneSettings::Observer {
 public:
  explicit NightLightClientImpl(
      const SimpleGeolocationProvider::Delegate* delegate,
      scoped_refptr<network::SharedURLLoaderFactory> factory);

  NightLightClientImpl(const NightLightClientImpl&) = delete;
  NightLightClientImpl& operator=(const NightLightClientImpl&) = delete;

  ~NightLightClientImpl() override;

  // ash::NightLightClient:
  void Start() override;

  // This class should respect the system geolocation permission. When the
  // permission is disabled, no requests should be dispatched and no responses
  // processed.
  // Called from `ash::Preferences::ApplyPreferences()`.
  void OnSystemGeolocationPermissionChanged(bool enabled) override;

  // ash::NightLightController::Observer:
  void OnScheduleTypeChanged(
      ash::NightLightController::ScheduleType new_type) override;

  // ash::system::TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  const base::OneShotTimer& timer() const { return *timer_; }

  base::Time last_successful_geo_request_time() const {
    return last_successful_geo_request_time_;
  }

  const std::u16string& current_timezone_id() const {
    return current_timezone_id_;
  }

  bool using_geoposition() const { return using_geoposition_; }

  static base::TimeDelta GetNextRequestDelayAfterSuccessForTesting();

  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer);

  void SetClockForTesting(const base::Clock* clock);

  void SetCurrentTimezoneIdForTesting(const std::u16string& timezone_id);

 protected:
  void OnGeoposition(const ash::Geoposition& position,
                     bool server_error,
                     const base::TimeDelta elapsed);

 private:
  base::Time GetNow() const;

  // Sends the most recent valid geoposition to NightLightController in ash.
  void SendCurrentGeoposition();

  void ScheduleNextRequest(base::TimeDelta delay);

  // ash::NightLightClient:
  void RequestGeoposition() override;

  // The IP-based geolocation provider.
  ash::SimpleGeolocationProvider provider_;

  raw_ptr<ash::NightLightController, ExperimentalAsh> night_light_controller_ =
      nullptr;

  // Delay after which a new request is retried after a failed one.
  base::TimeDelta backoff_delay_;

  std::unique_ptr<base::OneShotTimer> timer_;

  // Optional Used in tests to override the time of "Now".
  raw_ptr<const base::Clock, ExperimentalAsh> clock_ = nullptr;  // Not owned.

  // Last successful geoposition coordinates and its timestamp.
  base::Time last_successful_geo_request_time_;
  double latitude_ = 0.0;
  double longitude_ = 0.0;

  // The ID of the current timezone in the format similar to "America/Chicago".
  std::u16string current_timezone_id_;

  // True as long as the schedule type is set to "sunset to sunrise" or
  // "custom", which means this client will be retrieving the IP-based
  // geoposition once per day.
  bool using_geoposition_ = false;

  // System geolocation permission.
  bool system_geolocation_permission_enabled_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NIGHT_LIGHT_NIGHT_LIGHT_CLIENT_IMPL_H_

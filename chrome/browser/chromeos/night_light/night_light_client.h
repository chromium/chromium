// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NIGHT_LIGHT_NIGHT_LIGHT_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_NIGHT_LIGHT_NIGHT_LIGHT_CLIENT_H_

#include <memory>

#include "ash/public/cpp/night_light_controller.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/geolocation/simple_geolocation_provider.h"
#include "chromeos/settings/timezone_settings.h"

namespace base {
class Clock;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}

// Periodically requests the IP-based geolocation and provides it to the
// NightLightController running in ash.
class NightLightClient : public ash::NightLightController::Observer,
                         public chromeos::system::TimezoneSettings::Observer {
 public:
  explicit NightLightClient(
      scoped_refptr<network::SharedURLLoaderFactory> factory);
  ~NightLightClient() override;

  // Starts watching changes in the Night Light schedule type in order to begin
  // periodically pushing user's IP-based geoposition to NightLightController as
  // long as the type is set to "sunset to sunrise" or "custom".
  void Start();

  // ash::NightLightController::Observer:
  void OnScheduleTypeChanged(
      ash::NightLightController::ScheduleType new_type) override;

  // chromeos::system::TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  const base::OneShotTimer& timer() const { return *timer_; }

  base::Time last_successful_geo_request_time() const {
    return last_successful_geo_request_time_;
  }

  const base::string16& current_timezone_id() const {
    return current_timezone_id_;
  }

  bool using_geoposition() const { return using_geoposition_; }

  static base::TimeDelta GetNextRequestDelayAfterSuccessForTesting();

  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer);

  void SetClockForTesting(base::Clock* clock);

  void SetCurrentTimezoneIdForTesting(const base::string16& timezone_id);

 protected:
  void OnGeoposition(const chromeos::Geoposition& position,
                     bool server_error,
                     const base::TimeDelta elapsed);

 private:
  base::Time GetNow() const;

  // Sends the most recent valid geoposition to NightLightController in ash.
  void SendCurrentGeoposition();

  void ScheduleNextRequest(base::TimeDelta delay);

  // Virtual so that it can be overriden by a fake implementation in unit tests
  // that doesn't request actual geopositions.
  virtual void RequestGeoposition();

  // The IP-based geolocation provider.
  chromeos::SimpleGeolocationProvider provider_;

  ash::NightLightController* night_light_controller_ = nullptr;

  // Delay after which a new request is retried after a failed one.
  base::TimeDelta backoff_delay_;

  std::unique_ptr<base::OneShotTimer> timer_;

  // Optional Used in tests to override the time of "Now".
  base::Clock* clock_ = nullptr;  // Not owned.

  // Last successful geoposition coordinates and its timestamp.
  base::Time last_successful_geo_request_time_;
  double latitude_ = 0.0;
  double longitude_ = 0.0;

  // The ID of the current timezone in the fromat similar to "America/Chicago".
  base::string16 current_timezone_id_;

  // True as long as the schedule type is set to "sunset to sunrise" or
  // "custom", which means this client will be retrieving the IP-based
  // geoposition once per day.
  bool using_geoposition_ = false;

  DISALLOW_COPY_AND_ASSIGN(NightLightClient);
};

#endif  // CHROME_BROWSER_CHROMEOS_NIGHT_LIGHT_NIGHT_LIGHT_CLIENT_H_

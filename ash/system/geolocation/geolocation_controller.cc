// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/geolocation/geolocation_controller.h"

#include <algorithm>

#include "ash/shell.h"
#include "ash/system/time/time_of_day.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "third_party/icu/source/i18n/astro.h"

namespace ash {

namespace {

// Delay to wait for a response to our geolocation request, if we get a response
// after which, we will consider the request a failure.
constexpr base::TimeDelta kGeolocationRequestTimeout = base::Seconds(60);

// Minimum delay to wait to fire a new request after a previous one failing.
constexpr base::TimeDelta kMinimumDelayAfterFailure = base::Seconds(60);

// Delay to wait to fire a new request after a previous one succeeding.
constexpr base::TimeDelta kNextRequestDelayAfterSuccess = base::Days(1);

// Default sunset time at 6:00 PM as an offset from 00:00.
constexpr int kDefaultSunsetTimeOffsetMinutes = 18 * 60;

// Default sunrise time at 6:00 AM as an offset from 00:00.
constexpr int kDefaultSunriseTimeOffsetMinutes = 6 * 60;

}  // namespace

GeolocationController::GeolocationController(
    scoped_refptr<network::SharedURLLoaderFactory> factory)
    : factory_(factory.get()),
      provider_(std::move(factory),
                SimpleGeolocationProvider::DefaultGeolocationProviderURL()),
      backoff_delay_(kMinimumDelayAfterFailure),
      timer_(std::make_unique<base::OneShotTimer>()) {
  auto* timezone_settings = system::TimezoneSettings::GetInstance();
  current_timezone_id_ = timezone_settings->GetCurrentTimezoneID();
  timezone_settings->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

GeolocationController::~GeolocationController() {
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

// static
GeolocationController* GeolocationController::Get() {
  GeolocationController* controller =
      ash::Shell::Get()->geolocation_controller();
  DCHECK(controller);
  return controller;
}

void GeolocationController::AddObserver(Observer* observer) {
  const bool is_first_observer = observers_.empty();
  observers_.AddObserver(observer);
  if (is_first_observer)
    ScheduleNextRequest(base::Seconds(0));
}

void GeolocationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
  if (observers_.empty())
    timer_->Stop();
}

void GeolocationController::TimezoneChanged(const icu::TimeZone& timezone) {
  const std::u16string timezone_id =
      system::TimezoneSettings::GetTimezoneID(timezone);
  if (current_timezone_id_ == timezone_id)
    return;

  current_timezone_id_ = timezone_id;

  // On timezone changes, request an immediate geoposition.
  ScheduleNextRequest(base::Seconds(0));
}

void GeolocationController::SuspendDone(base::TimeDelta sleep_duration) {
  if (sleep_duration >= kNextRequestDelayAfterSuccess)
    ScheduleNextRequest(base::Seconds(0));
}

// static
base::TimeDelta
GeolocationController::GetNextRequestDelayAfterSuccessForTesting() {
  return kNextRequestDelayAfterSuccess;
}

void GeolocationController::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  timer_ = std::move(timer);
}

void GeolocationController::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void GeolocationController::SetCurrentTimezoneIdForTesting(
    const std::u16string& timezone_id) {
  current_timezone_id_ = timezone_id;
}

void GeolocationController::OnGeoposition(const Geoposition& position,
                                          bool server_error,
                                          const base::TimeDelta elapsed) {
  if (server_error || !position.Valid() ||
      elapsed > kGeolocationRequestTimeout) {
    VLOG(1) << "Failed to get a valid geoposition. Trying again later.";
    // Don't send invalid positions to ash.
    // On failure, we schedule another request after the current backoff delay.
    ScheduleNextRequest(backoff_delay_);

    // If another failure occurs next, our backoff delay should double.
    backoff_delay_ *= 2;
    return;
  }

  absl::optional<base::Time> previous_sunset;
  absl::optional<base::Time> previous_sunrise;
  bool possible_change_in_timezone = !geoposition_;
  if (geoposition_) {
    previous_sunset = GetSunsetTime();
    previous_sunrise = GetSunriseTime();
  }

  geoposition_ = std::make_unique<SimpleGeoposition>();
  geoposition_->latitude = position.latitude;
  geoposition_->longitude = position.longitude;

  if (previous_sunset && previous_sunrise) {
    // If the change in geoposition results in an hour or more in either sunset
    // or sunrise times indicates of a possible timezone change.
    constexpr base::TimeDelta kOneHourDuration = base::Hours(1);
    possible_change_in_timezone =
        (GetSunsetTime() - previous_sunset.value()).magnitude() >
            kOneHourDuration ||
        (GetSunriseTime() - previous_sunrise.value()).magnitude() >
            kOneHourDuration;
  }

  NotifyGeopositionChange(possible_change_in_timezone);

  // On success, reset the backoff delay to its minimum value, and schedule
  // another request.
  backoff_delay_ = kMinimumDelayAfterFailure;
  ScheduleNextRequest(kNextRequestDelayAfterSuccess);
}

base::Time GeolocationController::GetNow() const {
  return clock_ ? clock_->Now() : base::Time::Now();
}

void GeolocationController::ScheduleNextRequest(base::TimeDelta delay) {
  timer_->Start(FROM_HERE, delay, this,
                &GeolocationController::RequestGeoposition);
}

void GeolocationController::NotifyGeopositionChange(
    bool possible_change_in_timezone) {
  for (Observer& observer : observers_)
    observer.OnGeopositionChanged(possible_change_in_timezone);
}

void GeolocationController::RequestGeoposition() {
  VLOG(1) << "Requesting a new geoposition";
  provider_.RequestGeolocation(
      kGeolocationRequestTimeout, /*send_wifi_access_points=*/false,
      /*send_cell_towers=*/false,
      base::BindOnce(&GeolocationController::OnGeoposition,
                     base::Unretained(this)));
}

base::Time GeolocationController::GetSunRiseSet(bool sunrise) const {
  if (!geoposition_) {
    VLOG(1) << "Invalid geoposition. Using default time for "
            << (sunrise ? "sunrise." : "sunset.");
    return TimeOfDay(sunrise ? kDefaultSunriseTimeOffsetMinutes
                             : kDefaultSunsetTimeOffsetMinutes)
        .SetClock(clock_)
        .ToTimeToday();
  }

  icu::CalendarAstronomer astro(geoposition_->longitude,
                                geoposition_->latitude);
  // For sunset and sunrise times calculations to be correct, the time of the
  // icu::CalendarAstronomer object should be set to a time near local noon.
  // This avoids having the computation flopping over into an adjacent day.
  // See the documentation of icu::CalendarAstronomer::getSunRiseSet().
  // Note that the icu calendar works with milliseconds since epoch, and
  // base::Time::FromDoubleT() / ToDoubleT() work with seconds since epoch.
  const double midday_today_sec =
      TimeOfDay(12 * 60).SetClock(clock_).ToTimeToday().ToDoubleT();
  astro.setTime(midday_today_sec * 1000.0);
  const double sun_rise_set_ms = astro.getSunRiseSet(sunrise);
  return base::Time::FromDoubleT(sun_rise_set_ms / 1000.0);
}

}  // namespace ash

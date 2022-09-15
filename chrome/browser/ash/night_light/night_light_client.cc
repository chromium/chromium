// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/night_light/night_light_client.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {

// Delay to wait for a response to our geolocation request, if we get a response
// after which, we will consider the request a failure.
constexpr base::TimeDelta kGeolocationRequestTimeout = base::Seconds(60);

// Minimum delay to wait to fire a new request after a previous one failing.
constexpr base::TimeDelta kMinimumDelayAfterFailure = base::Seconds(60);

// Delay to wait to fire a new request after a previous one succeeding.
constexpr base::TimeDelta kNextRequestDelayAfterSuccess = base::Days(1);

}  // namespace

NightLightClient::NightLightClient(
    scoped_refptr<network::SharedURLLoaderFactory> factory)
    : provider_(
          std::move(factory),
          ash::SimpleGeolocationProvider::DefaultGeolocationProviderURL()),
      night_light_controller_(ash::NightLightController::GetInstance()),
      backoff_delay_(kMinimumDelayAfterFailure),
      timer_(std::make_unique<base::OneShotTimer>()) {}

NightLightClient::~NightLightClient() {
  if (night_light_controller_)
    night_light_controller_->RemoveObserver(this);
  ash::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void NightLightClient::Start() {
  auto* timezone_settings = ash::system::TimezoneSettings::GetInstance();
  current_timezone_id_ = timezone_settings->GetCurrentTimezoneID();
  timezone_settings->AddObserver(this);
  night_light_controller_->AddObserver(this);
}

void NightLightClient::OnScheduleTypeChanged(
    ash::NightLightController::ScheduleType new_type) {
  if (new_type == ash::NightLightController::ScheduleType::kNone) {
    using_geoposition_ = false;
    timer_->Stop();
    return;
  }

  using_geoposition_ = true;
  // No need to request a new position if we already have a valid one from a
  // request less than kNextRequestDelayAfterSuccess ago.
  base::Time now = GetNow();
  if ((now - last_successful_geo_request_time_) <
      kNextRequestDelayAfterSuccess) {
    VLOG(1) << "Already has a recent valid geoposition. Using it instead of "
            << "requesting a new one.";
    // Use the current valid position to update NightLightController.
    SendCurrentGeoposition();
  }

  // Next request is either immediate or kNextRequestDelayAfterSuccess later
  // than the last success time, whichever is greater.
  ScheduleNextRequest(std::max(
      base::Seconds(0),
      last_successful_geo_request_time_ + kNextRequestDelayAfterSuccess - now));
}

void NightLightClient::TimezoneChanged(const icu::TimeZone& timezone) {
  const std::u16string timezone_id =
      ash::system::TimezoneSettings::GetTimezoneID(timezone);
  if (current_timezone_id_ == timezone_id)
    return;

  current_timezone_id_ = timezone_id;

  if (!using_geoposition_)
    return;

  // On timezone changes, request an immediate geoposition.
  ScheduleNextRequest(base::Seconds(0));
}

// static
base::TimeDelta NightLightClient::GetNextRequestDelayAfterSuccessForTesting() {
  return kNextRequestDelayAfterSuccess;
}

void NightLightClient::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  timer_ = std::move(timer);
}

void NightLightClient::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void NightLightClient::SetCurrentTimezoneIdForTesting(
    const std::u16string& timezone_id) {
  current_timezone_id_ = timezone_id;
}

void NightLightClient::OnGeoposition(const ash::Geoposition& position,
                                     bool server_error,
                                     const base::TimeDelta elapsed) {
  if (!using_geoposition_) {
    // A response might arrive after the schedule type is no longer "sunset to
    // sunrise" or "custom", which means we should not push any positions to the
    // NightLightController.
    return;
  }

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

  last_successful_geo_request_time_ = GetNow();

  latitude_ = position.latitude;
  longitude_ = position.longitude;
  SendCurrentGeoposition();

  // On success, reset the backoff delay to its minimum value, and schedule
  // another request.
  backoff_delay_ = kMinimumDelayAfterFailure;
  ScheduleNextRequest(kNextRequestDelayAfterSuccess);
}

base::Time NightLightClient::GetNow() const {
  return clock_ ? clock_->Now() : base::Time::Now();
}

void NightLightClient::SendCurrentGeoposition() {
  night_light_controller_->SetCurrentGeoposition(
      ash::NightLightController::SimpleGeoposition{latitude_, longitude_});
}

void NightLightClient::ScheduleNextRequest(base::TimeDelta delay) {
  timer_->Start(FROM_HERE, delay, this, &NightLightClient::RequestGeoposition);
}

void NightLightClient::RequestGeoposition() {
  VLOG(1) << "Requesting a new geoposition";
  provider_.RequestGeolocation(
      kGeolocationRequestTimeout, false /* send_wifi_access_points */,
      false /* send_cell_towers */,
      base::BindOnce(&NightLightClient::OnGeoposition, base::Unretained(this)));
}

}  // namespace ash

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/geolocation/geolocation_controller.h"

#include <algorithm>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/time/time_of_day.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
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
      provider_(this,
                std::move(factory),
                SimpleGeolocationProvider::DefaultGeolocationProviderURL()),
      backoff_delay_(kMinimumDelayAfterFailure),
      timer_(std::make_unique<base::OneShotTimer>()),
      scoped_session_observer_(this) {
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

// static
void GeolocationController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDoublePref(prefs::kDeviceGeolocationCachedLatitude, 0.0);
  registry->RegisterDoublePref(prefs::kDeviceGeolocationCachedLongitude, 0.0);
}

void GeolocationController::AddObserver(Observer* observer) {
  const bool is_first_observer = observers_.empty();
  observers_.AddObserver(observer);
  if (is_first_observer && IsSystemGeolocationAllowed()) {
    ScheduleNextRequest(base::Seconds(0));
  }
}

void GeolocationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
  if (observers_.empty()) {
    timer_->Stop();
  }
}

void GeolocationController::TimezoneChanged(const icu::TimeZone& timezone) {
  const std::u16string timezone_id =
      system::TimezoneSettings::GetTimezoneID(timezone);
  if (current_timezone_id_ == timezone_id) {
    return;
  }

  current_timezone_id_ = timezone_id;

  // On timezone changes, request an immediate geoposition if the system
  // geolocation allows.
  if (IsSystemGeolocationAllowed()) {
    ScheduleNextRequest(base::Seconds(0));
  }
}

void GeolocationController::SuspendDone(base::TimeDelta sleep_duration) {
  if (sleep_duration >= kNextRequestDelayAfterSuccess) {
    ScheduleNextRequest(base::Seconds(0));
  }
}

bool GeolocationController::IsSystemGeolocationAllowed() const {
  // TODO(b/276715041): Refactor the `SimpleGeolocationProvider` class to
  // eliminate the `Shell`-dependency of this class.
  Shell* const shell = Shell::Get();
  const PrefService* primary_user_prefs =
      shell->session_controller()->GetPrimaryUserPrefService();

  // Follow device preference on log-in screen.
  if (!primary_user_prefs) {
    return shell->local_state()->GetInteger(
               ash::prefs::kDeviceGeolocationAllowed) ==
           static_cast<int>(PrivacyHubController::AccessLevel::kAllowed);
  }

  // Inside user session check geolocation user preference.
  return primary_user_prefs->GetBoolean(ash::prefs::kUserGeolocationAllowed);
}

void GeolocationController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_service == active_user_pref_service_.get()) {
    return;
  }

  active_user_pref_service_ = pref_service;
  LoadCachedGeopositionIfNeeded();
}

void GeolocationController::OnSystemGeolocationPermissionChanged(bool enabled) {
  // Drop all pending requests when system geolocation is toggled OFF.
  if (!enabled) {
    timer_->Stop();
    return;
  }

  // System geolocation toggled ON, post an immediate new geolocation request to
  // resume continuous scheduling.
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

void GeolocationController::SetLocalTimeConverterForTesting(
    const LocalTimeConverter* local_time_converter) {
  local_time_converter_ = local_time_converter;
}

void GeolocationController::SetCurrentTimezoneIdForTesting(
    const std::u16string& timezone_id) {
  current_timezone_id_ = timezone_id;
}

void GeolocationController::OnGeoposition(const Geoposition& position,
                                          bool server_error,
                                          const base::TimeDelta elapsed) {
  if (!IsSystemGeolocationAllowed() || observers_.empty()) {
    // The request might come after the user disabled the system geolocation
    // access or if all observers unsubscribed, in which case we should stop
    // processing the geolocation responses.
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

  is_current_geoposition_from_cache_ = false;
  StoreCachedGeoposition();

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
  // Drop all pending geolocation requests while system permission is
  // denied. Toggling system geolocation ON will trigger a fresh geolocation
  // request.
  if (!IsSystemGeolocationAllowed()) {
    return;
  }

  timer_->Start(FROM_HERE, delay, this,
                &GeolocationController::RequestGeoposition);
}

void GeolocationController::NotifyGeopositionChange(
    bool possible_change_in_timezone) {
  for (Observer& observer : observers_) {
    observer.OnGeopositionChanged(possible_change_in_timezone);
  }
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
        .SetLocalTimeConverter(local_time_converter_)
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
      TimeOfDay(12 * 60)
          .SetClock(clock_)
          .SetLocalTimeConverter(local_time_converter_)
          .ToTimeToday()
          .ToDoubleT();
  astro.setTime(midday_today_sec * 1000.0);
  const double sun_rise_set_ms = astro.getSunRiseSet(sunrise);
  // If there is 24 hours of daylight or darkness, `CalendarAstronomer` returns
  // a very large negative value. Any timestamp before or at the epoch
  // definitely does not make sense, so assume `kNoSunRiseSet`.
  return sun_rise_set_ms > 0 ? base::Time::FromDoubleT(sun_rise_set_ms / 1000.0)
                             : kNoSunRiseSet;
}

void GeolocationController::LoadCachedGeopositionIfNeeded() {
  DCHECK(active_user_pref_service_);

  // Even if there is a geoposition, but it's coming from a previously cached
  // value, switching users should load the currently saved values for the
  // new user. This is to keep users' prefs completely separate. We only ignore
  // the cached values once we have a valid non-cached geoposition from any
  // user in the same session.
  if (geoposition_ && !is_current_geoposition_from_cache_) {
    return;
  }

  if (!active_user_pref_service_->HasPrefPath(
          prefs::kDeviceGeolocationCachedLatitude) ||
      !active_user_pref_service_->HasPrefPath(
          prefs::kDeviceGeolocationCachedLongitude)) {
    LOG(ERROR)
        << "No valid current geoposition and no valid cached geoposition"
           " are available. Will use default times for sunset / sunrise.";
    geoposition_.reset();
    return;
  }

  geoposition_ = std::make_unique<SimpleGeoposition>();
  geoposition_->latitude = active_user_pref_service_->GetDouble(
      prefs::kDeviceGeolocationCachedLatitude);
  geoposition_->longitude = active_user_pref_service_->GetDouble(
      prefs::kDeviceGeolocationCachedLongitude);
  is_current_geoposition_from_cache_ = true;
}

void GeolocationController::StoreCachedGeoposition() const {
  CHECK(geoposition_);
  const SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  for (const auto& user_session : session_controller->GetUserSessions()) {
    PrefService* pref_service = session_controller->GetUserPrefServiceForUser(
        user_session->user_info.account_id);
    if (!pref_service) {
      continue;
    }

    pref_service->SetDouble(prefs::kDeviceGeolocationCachedLatitude,
                            geoposition_->latitude);
    pref_service->SetDouble(prefs::kDeviceGeolocationCachedLongitude,
                            geoposition_->longitude);
  }
}

}  // namespace ash

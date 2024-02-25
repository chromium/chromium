// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_GEOLOCATION_GEOLOCATION_CONTROLLER_H_
#define ASH_SYSTEM_GEOLOCATION_GEOLOCATION_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/dbus/power/power_manager_client.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace ash {

class LocalTimeConverter;

// Represents a geolocation position fix. It's "simple" because it doesn't
// expose all the parameters of the position interface as defined by the
// Geolocation API Specification:
//   https://dev.w3.org/geo/api/spec-source.html#position_interface
// The GeolocationController is only interested in valid latitude and
// longitude. It also doesn't require any specific accuracy. The more accurate
// the positions, the more accurate sunset and sunrise times calculations.
// However, an IP-based geoposition is considered good enough.
struct SimpleGeoposition {
  bool operator==(const SimpleGeoposition& other) const {
    return latitude == other.latitude && longitude == other.longitude;
  }
  double latitude;
  double longitude;
};

// Periodically requests the IP-based geolocation and provides it to the
// observers, `GeolocationController::Observer`. This class also observes
// timezone changes to request a new geoposition.
// TODO(crbug.com/1272178): `GeolocationController` should observe the sleep
// and update next request time.
class ASH_EXPORT GeolocationController
    : public SimpleGeolocationProvider::Observer,
      public system::TimezoneSettings::Observer,
      public chromeos::PowerManagerClient::Observer,
      public SessionObserver {
 public:
  // Possible errors for `GetSunsetTime()` and `GetSunriseTime()`.
  enum class SunRiseSetError {
    // The current geolocation has no sunrise/sunset (24 hours of daylight or
    // darkness).
    kNoSunRiseSet,
    // Sunrise/set are temporarily unavailable, including the default values of
    // 6 AM/PM local time. Caller should handle this gracefully and try again
    // later.
    kUnavailable
  };

  static constexpr base::expected<base::Time, SunRiseSetError> kNoSunRiseSet =
      base::unexpected(SunRiseSetError::kNoSunRiseSet);
  static constexpr base::expected<base::Time, SunRiseSetError>
      kSunRiseSetUnavailable = base::unexpected(SunRiseSetError::kUnavailable);

  class Observer : public base::CheckedObserver {
   public:
    // Emitted when the Geoposition is updated with
    // |possible_change_in_timezone| to indicate whether timezone might have
    // changed as a result of the geoposition change.
    virtual void OnGeopositionChanged(bool possible_change_in_timezone) {}

   protected:
    ~Observer() override = default;
  };

  explicit GeolocationController(SimpleGeolocationProvider* const provider);
  GeolocationController(const GeolocationController&) = delete;
  GeolocationController& operator=(const GeolocationController&) = delete;
  ~GeolocationController() override;

  static GeolocationController* Get();
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  const base::OneShotTimer& timer() const { return *timer_; }

  const std::u16string& current_timezone_id() const {
    return current_timezone_id_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // SimpleGeolocationProvider::Observer:
  void OnGeolocationPermissionChanged(bool enabled) override;

  // system::TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // Returns sunset and sunrise time calculated from the most recently observed
  // geoposition. If a geoposition has not been observed, defaults to sunset
  // 6 PM and sunrise 6 AM.
  base::expected<base::Time, SunRiseSetError> GetSunsetTime() const {
    return GetSunRiseSet(/*sunrise=*/false);
  }
  base::expected<base::Time, SunRiseSetError> GetSunriseTime() const {
    return GetSunRiseSet(/*sunrise=*/true);
  }

  static base::TimeDelta GetNextRequestDelayAfterSuccessForTesting();

  base::OneShotTimer* GetTimerForTesting() { return timer_.get(); }

  bool HasObserver(const Observer* obs) const {
    return observers_.HasObserver(obs);
  }

  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer);

  void SetClockForTesting(base::Clock* clock);
  void SetLocalTimeConverterForTesting(
      const LocalTimeConverter* local_time_converter);
  void SetCurrentTimezoneIdForTesting(const std::u16string& timezone_id);
  // Resets the running `timer_` and issues an immediate geoposition request.
  // Any responses on the fly will be processed first, but will be overridden
  // once the response of this request arrives.
  void RequestImmediateGeopositionForTesting();

 protected:
  // The callback of geolocation request via `provider_`. Once receiving a
  // new position, it `NotifyWithCurrentGeoposition()` to broadcast the position
  // to observers and `ScheduleNextRequest()` on the next day. If the retrieval
  // fails, it `ScheduleNextRequest()` after a `backoff_delay_`, which is
  // doubled for each failure.
  void OnGeoposition(const Geoposition& position,
                     bool server_error,
                     const base::TimeDelta elapsed);

  // Virtual so that it can be overridden by a fake implementation in unit tests
  // that doesn't request actual geopositions.
  virtual void RequestGeoposition();

 private:
  // Calls `RequestGeoposition()` after `delay`.
  void ScheduleNextRequest(base::TimeDelta delay);

  // Broadcasts the change in geoposition to all observers with
  // |possible_change_in_timezone| to indicate whether timezone might have
  // changed as a result of the geoposition change.
  void NotifyGeopositionChange(bool possible_change_in_timezone);

  // Note that the below computation is intentionally performed every time
  // GetSunsetTime() or GetSunriseTime() is called rather than once whenever we
  // receive a geoposition (which happens at least once a day). This reduces
  // the chances of getting inaccurate values, especially around DST changes.
  base::expected<base::Time, SunRiseSetError> GetSunRiseSet(bool sunrise) const;

  // Called only when the active user changes in order to see if we need to use
  // a previously cached geoposition value from the active user's prefs.
  void LoadCachedGeopositionIfNeeded();

  // Called whenever we receive a new geoposition update to cache it in all
  // logged-in users' prefs so that it can be used later in the event of not
  // being able to retrieve a valid geoposition.
  void StoreCachedGeoposition() const;

  // Points to the `SimpleGeolocationProvider::GetInstance()` throughout the
  // object lifecycle. Overridden in unit tests.
  raw_ptr<SimpleGeolocationProvider> geolocation_provider_ = nullptr;

  // May be null if a user has not logged in yet.
  raw_ptr<PrefService> active_user_pref_service_ = nullptr;
  std::unique_ptr<PrefChangeRegistrar> registrar_;

  // Delay after which a new request is retried after a failed one.
  base::TimeDelta backoff_delay_;

  std::unique_ptr<base::OneShotTimer> timer_;

  // Optional Used in tests to override the time of "Now".
  raw_ptr<base::Clock> clock_ = nullptr;  // Not owned.

  // Optional Used in tests to override all local time operations.
  raw_ptr<const LocalTimeConverter> local_time_converter_ =
      nullptr;  // Not owned.

  // The ID of the current timezone in the format similar to "America/Chicago".
  std::u16string current_timezone_id_;

  base::ObserverList<Observer> observers_;

  // True if the current `geoposition_` is from a previously cached value in the
  // user prefs of any of the users in the current session. It is reset to false
  // once we receive a newly-updated geoposition. This is used to treat the
  // current geoposition as temporary until we receive a valid geoposition
  // update, and also not to let a cached geoposition value to leak to another
  // user for privacy reasons.
  bool is_current_geoposition_from_cache_ = false;

  std::unique_ptr<SimpleGeoposition> geoposition_;

  ScopedSessionObserver scoped_session_observer_;

  base::WeakPtrFactory<GeolocationController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_GEOLOCATION_GEOLOCATION_CONTROLLER_H_

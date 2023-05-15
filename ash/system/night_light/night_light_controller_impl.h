// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/night_light_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/time/time_of_day.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/aura/env_observer.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class PrefRegistrySimple;
class PrefService;

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

class ColorTemperatureAnimation;

// Controls the NightLight feature that adjusts the color temperature of the
// screen. It uses the display's hardware CRTC (Cathode Ray Tube Controller)
// color transform matrix (CTM) when possible for efficiency, and can fall back
// to setting a color matrix on the compositor if the display doesn't support
// color transformation.
// For Unified Desktop mode, the color matrix is set on the mirroring actual
// displays' hosts, rather than on the Unified host, so that we can use the
// CRTC matrix if available (the Unified host doesn't correspond to an actual
// display).
class ASH_EXPORT NightLightControllerImpl
    : public NightLightController,
      public WindowTreeHostManager::Observer,
      public aura::EnvObserver,
      public SessionObserver,
      public chromeos::PowerManagerClient::Observer,
      public message_center::NotificationObserver {
 public:
  enum class AnimationDuration {
    // Short animation (2 seconds) used for manual changes of NightLight status
    // and temperature by the user.
    kShort,

    // Long animation (20 seconds) used for applying the color temperature
    // gradually as a result of getting into or out of the automatically
    // scheduled NightLight mode. This gives the user a smooth transition.
    kLong,
  };

  // This class enables us to inject fake values for "Now" as well as the sunset
  // and sunrise times, so that we can reliably test the behavior in various
  // schedule types and times.
  class Delegate {
   public:
    // NightLightController owns the delegate.
    virtual ~Delegate() = default;

    // Gets the current time.
    virtual base::Time GetNow() const = 0;

    // Gets the sunset and sunrise times.
    virtual base::Time GetSunsetTime() const = 0;
    virtual base::Time GetSunriseTime() const = 0;

    // Provides the delegate with the geoposition so that it can be used to
    // calculate sunset and sunrise times.
    // Returns true if |position| is different than the current known value,
    // potentially requiring a refresh of the schedule. False otherwise.
    virtual bool SetGeoposition(const SimpleGeoposition& position) = 0;

    // Returns true if a geoposition value is available.
    virtual bool HasGeoposition() const = 0;
  };

  NightLightControllerImpl();

  NightLightControllerImpl(const NightLightControllerImpl&) = delete;
  NightLightControllerImpl& operator=(const NightLightControllerImpl&) = delete;

  ~NightLightControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Convenience functions for converting between the color temperature value,
  // and the blue and green color scales. Note that the red color scale remains
  // unaffected (i.e. its scale remains 1.0f);
  // When these color scales are to be applied in the linear color space (i.e.
  // after gamma decoding), |temperature| should be the non-linear temperature
  // (see GetNonLinearTemperature() below), the blue scale uses the same
  // attenuation, while the green scale is attenuated a bit more than it
  // normally is when the scales are meant for the compressed gamma space.
  static float BlueColorScaleFromTemperature(float temperature);
  static float GreenColorScaleFromTemperature(float temperature,
                                              bool in_linear_space);

  // When using the CRTC color correction, depending on the hardware, the matrix
  // may be applied in the linear gamma space (i.e. after gamma decoding), or in
  // the non-linear gamma compressed space (i.e. after degamma encoding). Our
  // standard temperature we use here, which the user changes, follow a linear
  // slope from 0.0f to 1.0f. This won't give the same linear rate of change in
  // colors as the temperature changes in the linear color space. To account for
  // this, we want the temperature to follow the same slope as that of the gamma
  // factor.
  // This function returns the non-linear temperature that corresponds to the
  // linear |temperature| value.
  static float GetNonLinearTemperature(float temperature);

  // When reading ambient color temperature via powerd, it needs to be mapped
  // to another temperature before it can be used to determine the RGB scale
  // factors (i.e: CTM diagonal).  The mapping was computed according to
  // internal user studies.
  // The returned adjusted temperature is in Kelvin as well.
  static float RemapAmbientColorTemperature(float temperature_in_kelvin);

  // Given an overall temperature in Kelvin, returns the scale factors for R, G
  // and B channel.
  // |temperature_in_kelvin| is expected to be a remapped color temperature
  // from the sensor using |RemapAmbientColorTemperature|.
  static gfx::Vector3dF ColorScalesFromRemappedTemperatureInKevin(
      float temperature_in_kelvin);

  AnimationDuration animation_duration() const { return animation_duration_; }
  AnimationDuration last_animation_duration() const {
    return last_animation_duration_;
  }
  base::OneShotTimer* timer() { return &timer_; }
  bool is_current_geoposition_from_cache() const {
    return is_current_geoposition_from_cache_;
  }
  float ambient_temperature() const { return ambient_temperature_; }
  const gfx::Vector3dF& ambient_rgb_scaling_factors() const {
    return ambient_rgb_scaling_factors_;
  }

  float GetColorTemperature() const;
  ScheduleType GetScheduleType() const;
  TimeOfDay GetCustomStartTime() const;
  TimeOfDay GetCustomEndTime() const;
  bool GetAmbientColorEnabled() const;

  // Get whether the current time is after sunset and before sunrise.
  bool IsNowWithinSunsetSunrise() const;

  // Update |ambient_rgb_scaling_factors_| from the current
  // |ambient_temperature_|.
  void UpdateAmbientRgbScalingFactors();

  // Set the desired NightLight settings in the current active user prefs.
  void SetEnabled(bool enabled, AnimationDuration animation_type);
  void SetColorTemperature(float temperature);
  void SetScheduleType(ScheduleType type);
  void SetCustomStartTime(TimeOfDay start_time);
  void SetCustomEndTime(TimeOfDay end_time);
  void SetAmbientColorEnabled(bool enabled);

  // This is always called as a result of a user action and will always use the
  // AnimationDurationType::kShort.
  void Toggle();

  // ash::WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {}
  void OnHostInitialized(aura::WindowTreeHost* host) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // ash::NightLightController:
  void SetCurrentGeoposition(const SimpleGeoposition& position) override;
  bool GetEnabled() const override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;
  void AmbientColorChanged(const int32_t color_temperature) override;

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate);

  // Returns the Auto Night Light notification if any is currently shown, or
  // nullptr.
  message_center::Notification* GetAutoNightLightNotificationForTesting() const;

 private:
  // Attempts restoring a previously stored schedule for the current user if
  // possible and returns true if so, false otherwise.
  bool MaybeRestoreSchedule();

  // Returns true if the user has ever changed the schedule type, which means we
  // must respect the user's choice and let it overwrite Auto Night Light.
  bool UserHasEverChangedSchedule() const;

  // Returns true if the user has ever dismissed the Auto Night Light
  // notification, in which case we never show it again.
  bool UserHasEverDismissedAutoNightLightNotification() const;

  // Shows the notification informing the user that Night Light has been turned
  // on from sunset-to-sunrise as a result of Auto Night Light.
  void ShowAutoNightLightNotification();

  // Disables showing the Auto Night Light from now on.
  void DisableShowingFutureAutoNightLightNotification();

  // Called only when the active user changes in order to see if we need to use
  // a previously cached geoposition value from the active user's prefs.
  void LoadCachedGeopositionIfNeeded();

  // Called whenever we receive a new geoposition update to cache it in all
  // logged-in users' prefs so that it can be used later in the event of not
  // being able to retrieve a valid geoposition.
  void StoreCachedGeoposition(const SimpleGeoposition& position);

  // Refreshes the displays color transforms based on the given
  // |color_temperature|, which will be overridden to a value of 0 if NightLight
  // is turned off.
  void RefreshDisplaysTemperature(float color_temperature);

  // Reapplys the current color temperature on the displays without starting a
  // new animation or overriding an on-going one towards the same target
  // temperature.
  void ReapplyColorTemperatures();

  void StartWatchingPrefsChanges();

  void InitFromUserPrefs();

  void NotifyStatusChanged();

  void NotifyClientWithScheduleChange();

  // Called when the user pref for the enabled status of NightLight is changed.
  void OnEnabledPrefChanged();

  // Called when the user pref for the enabled status of Ambient Color is
  // changed.
  void OnAmbientColorEnabledPrefChanged();

  // Called when the user pref for the color temperature is changed.
  void OnColorTemperaturePrefChanged();

  // Called when the user pref for the schedule type is changed.
  void OnScheduleTypePrefChanged();

  // Called when either of the custom schedule prefs (custom start or end times)
  // are changed.
  void OnCustomSchedulePrefsChanged();

  // Refreshes the state of NightLight according to the currently set
  // parameters. |did_schedule_change| is true when Refresh() is called as a
  // result of a change in one of the schedule related prefs, and false
  // otherwise.
  // If |keep_manual_toggles_during_schedules| is true, refreshing the schedule
  // will not override a previous user's decision to toggle the NightLight
  // status while the schedule is being used.
  void Refresh(bool did_schedule_change,
               bool keep_manual_toggles_during_schedules);

  // Given the desired start and end times that determine the time interval
  // during which NightLight will be ON, depending on the time of "now", it
  // refreshes the |timer_| to either schedule the future start or end of
  // NightLight mode, as well as update the current status if needed.
  // For |did_schedule_change| and |keep_manual_toggles_during_schedules|, see
  // Refresh() above.
  // This function should never be called if the schedule type is |kNone|.
  void RefreshScheduleTimer(base::Time start_time,
                            base::Time end_time,
                            bool did_schedule_change,
                            bool keep_manual_toggles_during_schedules);

  // Schedule the upcoming next toggle of NightLight mode. This is used for the
  // automatic status changes of NightLight which always use an
  // AnimationDurationType::kLong.
  void ScheduleNextToggle(base::TimeDelta delay);

  std::unique_ptr<Delegate> delegate_;

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  raw_ptr<PrefService, ExperimentalAsh> active_user_pref_service_ = nullptr;

  // The animation duration of any upcoming future change.
  AnimationDuration animation_duration_ = AnimationDuration::kShort;
  // The animation duration of the change that was just performed.
  AnimationDuration last_animation_duration_ = AnimationDuration::kShort;

  std::unique_ptr<ColorTemperatureAnimation> temperature_animation_;

  // Tracks the upcoming NightLight state changes per each user due to automatic
  // schedules. This can be used to restore a manually toggled status while the
  // schedule is being used. See MaybeRestoreSchedule().
  struct ScheduleTargetState {
    // The time at which NightLight will switch to |target_status| defined
    // below.
    base::Time target_time;
    bool target_status;
  };
  base::flat_map<PrefService*, ScheduleTargetState>
      per_user_schedule_target_state_;

  // The timer that schedules the start and end of NightLight when the schedule
  // type is either kSunsetToSunrise or kCustom.
  base::OneShotTimer timer_;

  // True only until Night Light is initialized from the very first user
  // session. After that, it is set to false.
  bool is_first_user_init_ = true;

  // True if the current geoposition value used by the Delegate is from a
  // previously cached value in the user prefs of any of the users in the
  // current session. It is reset to false once we receive a newly-updated
  // geoposition from the client.
  // This is used to treat the current geoposition as temporary until we receive
  // a valid geoposition update, and also not to let a cached geoposition value
  // to leak to another user for privacy reasons.
  bool is_current_geoposition_from_cache_ = false;

  // The registrar used to watch NightLight prefs changes in the above
  // |active_user_pref_service_| from outside ash.
  // NOTE: Prefs are how Chrome communicates changes to the NightLight settings
  // controlled by this class from the WebUI settings.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Last ambient temperature read from the sensor. It is continuously
  // updated for every new value even when GetAmbientColorEnabled() returns
  // false.
  float ambient_temperature_;

  // The ambient color R, G, and B scaling factors.
  // Valid only if ambient color is enabled.
  gfx::Vector3dF ambient_rgb_scaling_factors_ = {1.f, 1.f, 1.f};

  base::WeakPtrFactory<NightLightControllerImpl> weak_ptr_factory_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_IMPL_H_

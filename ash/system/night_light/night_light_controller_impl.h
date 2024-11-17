// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/night_light_controller.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/night_light/night_light_metrics_recorder.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "ash/system/time/time_of_day.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/moving_window.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/aura/env_observer.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class PrefRegistrySimple;

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
      public display::DisplayManagerObserver,
      public aura::EnvObserver,
      public message_center::NotificationObserver,
      public ScheduledFeature {
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

  NightLightControllerImpl();

  NightLightControllerImpl(const NightLightControllerImpl&) = delete;
  NightLightControllerImpl& operator=(const NightLightControllerImpl&) = delete;

  ~NightLightControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Convenience functions for converting between the color temperature value,
  // and the blue and green color scales. Note that the red color scale remains
  // unaffected (i.e. its scale remains 1.0f);
  static float BlueColorScaleFromTemperature(float temperature);
  static float GreenColorScaleFromTemperature(float temperature);

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
  float ambient_temperature() const { return ambient_temperature_; }
  const gfx::Vector3dF& ambient_rgb_scaling_factors() const {
    return ambient_rgb_scaling_factors_;
  }

  float GetColorTemperature() const;
  bool GetAmbientColorEnabled() const;

  // Update |ambient_rgb_scaling_factors_| from the current
  // |ambient_temperature_|.
  void UpdateAmbientRgbScalingFactors();

  // Set the desired NightLight settings in the current active user prefs.
  void SetColorTemperature(float temperature);
  void SetAmbientColorEnabled(bool enabled);

  // This is always called as a result of a user action and will always use the
  // AnimationDurationType::kShort.
  void Toggle();

  // ui::display::DisplayManagerObserver:
  void OnDidApplyDisplayChanges() override;

  // aura::EnvObserver:
  void OnHostInitialized(aura::WindowTreeHost* host) override;

  // ash::NightLightController:
  bool IsNightLightEnabled() const override;

  // chromeos::PowerManagerClient::Observer:
  void AmbientColorChanged(const int32_t color_temperature) override;

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

  // Returns the Auto Night Light notification if any is currently shown, or
  // nullptr.
  message_center::Notification* GetAutoNightLightNotificationForTesting() const;

 private:
  // ScheduledFeature:
  void RefreshFeatureState(RefreshReason reason) override;
  const char* GetFeatureName() const override;
  void InitFeatureForNewActiveUser() override;
  void ListenForPrefChanges(
      PrefChangeRegistrar& pref_change_registrar) override;
  const char* GetScheduleTypeHistogramName() const override;

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

  // Refreshes the displays color transforms based on the given
  // |color_temperature|, which will be overridden to a value of 0 if NightLight
  // is turned off.
  void RefreshDisplaysTemperature(float color_temperature);

  // Reapplys the current color temperature on the displays without starting a
  // new animation or overriding an on-going one towards the same target
  // temperature.
  void ReapplyColorTemperatures();

  void NotifyStatusChanged();

  void NotifyClientWithScheduleChange();

  // Called when the user pref for the enabled status of Ambient Color is
  // changed.
  void OnAmbientColorEnabledPrefChanged();

  // Called when the user pref for the color temperature is changed.
  void OnColorTemperaturePrefChanged();

  void UpdateAutoNightLightNotification(RefreshReason refresh_reason);

  // The animation duration of any upcoming future change.
  AnimationDuration animation_duration_ = AnimationDuration::kShort;
  // The animation duration of the change that was just performed.
  AnimationDuration last_animation_duration_ = AnimationDuration::kShort;

  std::unique_ptr<ColorTemperatureAnimation> temperature_animation_;

  std::unique_ptr<NightLightMetricsRecorder> night_light_metrics_recorder_;

  // True only until Night Light is initialized from the very first user
  // session. After that, it is set to false.
  bool is_first_user_init_ = true;

  // Moving average of ambient temperature read from the sensor. It is
  // continuously updated for every new value even when GetAmbientColorEnabled()
  // returns false.
  base::MovingAverage<float, float> ambient_temperature_sensor_values_;

  // The current ambient temperature being applied.
  float ambient_temperature_;

  // The ambient color R, G, and B scaling factors.
  // Valid only if ambient color is enabled.
  gfx::Vector3dF ambient_rgb_scaling_factors_ = {1.f, 1.f, 1.f};

  // Night light state in the last call to `RefreshFeatureState()`. `nullopt`
  // if no call has been made yet.
  std::optional<bool> last_observed_enabled_state_;

  base::WeakPtrFactory<NightLightControllerImpl> weak_ptr_factory_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_IMPL_H_

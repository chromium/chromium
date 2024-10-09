// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/system/night_light/night_light_controller_impl.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/display/display_color_manager.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/night_light/night_light_metrics_recorder.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "cc/base/math_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/i18n/astro.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/skia_color_space_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

// Defines the states of the Auto Night Light notification as a result of a
// user's interaction with it.
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "AshAutoNightLightNotificationState" in
// src/tools/metrics/histograms/metadata/ash/enums.xml.
enum class AutoNightLightNotificationState {
  kClosedByUser = 0,
  kBodyClicked = 1,
  kButtonClickedDeprecated = 2,
  kMaxValue = kButtonClickedDeprecated,
};

// The name of the histogram reporting the state of the user's interaction with
// the Auto Night Light notification.
constexpr char kAutoNightLightNotificationStateHistogram[] =
    "Ash.NightLight.AutoNightLightNotificationState";

// The name of a boolean histogram logging when the Auto Night Light
// notification is shown.
constexpr char kAutoNightLightNotificationShownHistogram[] =
    "Ash.NightLight.AutoNightLightNotificationShown";

// Auto Night Light notification IDs.
constexpr char kNotifierId[] = "ash.night_light_controller_impl";
constexpr char kNotificationId[] = "ash.auto_night_light_notify";

// Default start time at 6:00 PM as an offset from 00:00.
constexpr int kDefaultStartTimeOffsetMinutes = 18 * 60;

// Default end time at 6:00 AM as an offset from 00:00.
constexpr int kDefaultEndTimeOffsetMinutes = 6 * 60;

constexpr float kDefaultColorTemperature = 0.5f;

// The duration of the temperature change animation for
// AnimationDurationType::kShort.
constexpr base::TimeDelta kManualAnimationDuration = base::Seconds(1);

// The duration of the temperature change animation for
// AnimationDurationType::kLong.
constexpr base::TimeDelta kAutomaticAnimationDuration = base::Seconds(60);

// The size of the window for color temperature moving average calculations.
constexpr unsigned long kMovingAverageWindowSize = 20u;

// The color temperature animation frames per second.
constexpr int kNightLightAnimationFrameRate = 15;

// The following are color temperatues in Kelvin.
// The min/max are a reasonable range we can clamp the values to.
constexpr float kMinColorTemperatureInKelvin = 4500;
constexpr float kNeutralColorTemperatureInKelvin = 6500;
constexpr float kMaxColorTemperatureInKelvin = 7500;

// Returns the color temperature range bucket in which |temperature| resides.
// The range buckets are:
// 0 => Range [0 : 20) (least warm).
// 1 => Range [20 : 40).
// 2 => Range [40 : 60).
// 3 => Range [60 : 80).
// 4 => Range [80 : 100] (most warm).
int GetTemperatureRange(float temperature) {
  return std::clamp(std::floor(5 * temperature), 0.0f, 4.0f);
}

// Returns the color matrix that corresponds to the given |temperature|.
// The matrix will be affected by the current |ambient_temperature_| if
// |apply_ambient_temperature| is true. This matrix should be applied to
// sRGB-encoded colors.
SkM44 MatrixFromTemperature(float temperature, bool apply_ambient_temperature) {
  SkM44 matrix;
  if (temperature != 0.0f) {
    const float blue_scale =
        NightLightControllerImpl::BlueColorScaleFromTemperature(temperature);
    const float green_scale =
        NightLightControllerImpl::GreenColorScaleFromTemperature(temperature);

    matrix.setRC(1, 1, green_scale);
    matrix.setRC(2, 2, blue_scale);
  }

  auto* night_light_controller = Shell::Get()->night_light_controller();
  DCHECK(night_light_controller);
  if (apply_ambient_temperature) {
    const gfx::Vector3dF& ambient_rgb_scaling_factors =
        night_light_controller->ambient_rgb_scaling_factors();

    // Multiply the two scale factors.
    // If either night light or ambient EQ are disabled the CTM will be affected
    // only by the enabled effect.
    matrix.setRC(0, 0, ambient_rgb_scaling_factors.x());
    matrix.setRC(1, 1, matrix.rc(1, 1) * ambient_rgb_scaling_factors.y());
    matrix.setRC(2, 2, matrix.rc(2, 2) * ambient_rgb_scaling_factors.z());
  }

  return matrix;
}

// Based on the result of setting the hardware CRTC matrix |crtc_matrix_result|,
// either apply the |night_light_matrix| on the compositor, or reset it to
// the identity matrix to avoid having double the Night Light effect.
void UpdateCompositorMatrix(aura::WindowTreeHost* host,
                            const SkM44& night_light_matrix,
                            bool crtc_matrix_result) {
  if (host->compositor()) {
    host->compositor()->SetDisplayColorMatrix(
        crtc_matrix_result ? SkM44() : night_light_matrix);
  }
}

// Attempts setting the given color matrix on the display hardware of
// |display_id|. The matrix `gamma_compressed_matrix` will be applied
// in gamma space. Returns true if the hardware supports this operation
// the matrix was successfully sent to the GPU.
bool AttemptSettingHardwareCtm(int64_t display_id,
                               const SkM44& gamma_compressed_matrix) {
  display::ColorTemperatureAdjustment ctm;
  ctm.srgb_matrix = gfx::SkcmsMatrix3x3FromSkM44(gamma_compressed_matrix);
  return Shell::Get()
      ->display_color_manager()
      ->SetDisplayColorTemperatureAdjustment(display_id, ctm);
}

// Applies the given |temperature| to the display associated with the given
// |host|. This is useful for when we have a host and not a display ID.
// The final color transform computed from the temperature, will be affected
// by the current |ambient_temperature_| if GetAmbientColorEnabled() returns
// true.
void ApplyTemperatureToHost(aura::WindowTreeHost* host, float temperature) {
  DCHECK(host);
  const int64_t display_id = host->GetDisplayId();
  DCHECK_NE(display_id, display::kInvalidDisplayId);
  if (display_id == display::kUnifiedDisplayId) {
    // In Unified Desktop mode, applying the color matrix to either the CRTC or
    // the compositor of the mirroring (actual) displays is sufficient. If we
    // apply it to the compositor of the Unified host (since there's no actual
    // display CRTC for |display::kUnifiedDisplayId|), then we'll end up with
    // double the temperature.
    return;
  }

  auto* night_light_controller = Shell::Get()->night_light_controller();
  DCHECK(night_light_controller);

  // Only apply ambient EQ to internal displays.
  const bool apply_ambient_temperature =
      night_light_controller->GetAmbientColorEnabled() &&
      display::IsInternalDisplayId(display_id);

  const SkM44 gamma_compressed_matrix =
      MatrixFromTemperature(temperature, apply_ambient_temperature);
  const bool crtc_result =
      AttemptSettingHardwareCtm(display_id, gamma_compressed_matrix);
  UpdateCompositorMatrix(host, gamma_compressed_matrix, crtc_result);
}

// Applies the given |temperature| value by converting it to the corresponding
// color matrix that will be set on the output displays.
// The final color transform computed from the temperature, will be affected
// by the current |ambient_temperature_| if GetAmbientColorEnabled() returns
// true.
void ApplyTemperatureToAllDisplays(float temperature) {
  Shell* shell = Shell::Get();
  WindowTreeHostManager* wth_manager = shell->window_tree_host_manager();
  for (int64_t display_id :
       shell->display_manager()->GetConnectedDisplayIdList()) {
    DCHECK_NE(display_id, display::kUnifiedDisplayId);

    aura::Window* root_window =
        wth_manager->GetRootWindowForDisplayId(display_id);
    if (!root_window) {
      // Some displays' hosts may have not being initialized yet. In this case
      // NightLightControllerImpl::OnHostInitialized() will take care of those
      // hosts.
      continue;
    }

    auto* host = root_window->GetHost();
    DCHECK(host);
    ApplyTemperatureToHost(host, temperature);
  }
}

void VerifyAmbientColorCtmSupport() {
  // TODO(dcastagna): Move this function and call it from
  // DisplayColorManager::OnDisplayModeChanged()
  Shell* shell = Shell::Get();
  const DisplayColorManager::DisplayCtmSupport displays_ctm_support =
      shell->display_color_manager()->displays_ctm_support();
  if (displays_ctm_support != DisplayColorManager::DisplayCtmSupport::kAll) {
    LOG(ERROR) << "When ambient color mode is enabled, all the displays must "
                  "support CTMs.";
  }
}

}  // namespace

// Defines a linear animation type to animate the color temperature between two
// values in a given time duration. The color temperature is animated when
// NightLight changes status from ON to OFF or vice versa, whether this change
// is automatic (via the automatic schedule) or manual (user initiated).
class ColorTemperatureAnimation : public gfx::LinearAnimation,
                                  public gfx::AnimationDelegate {
 public:
  ColorTemperatureAnimation()
      : gfx::LinearAnimation(kManualAnimationDuration,
                             kNightLightAnimationFrameRate,
                             this) {}
  ColorTemperatureAnimation(const ColorTemperatureAnimation&) = delete;
  ColorTemperatureAnimation& operator=(const ColorTemperatureAnimation&) =
      delete;
  ~ColorTemperatureAnimation() override = default;

  float target_temperature() const { return target_temperature_; }

  // Starts a new temperature animation from the |current_temperature_| to the
  // given |new_target_temperature| in the given |duration|.
  void AnimateToNewValue(float new_target_temperature,
                         base::TimeDelta duration) {
    if (cc::MathUtil::IsWithinEpsilon(current_temperature_,
                                      new_target_temperature)) {
      return;
    }

    start_temperature_ = current_temperature_;
    target_temperature_ = std::clamp(new_target_temperature, 0.0f, 1.0f);

    if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
      // Animations are disabled. Apply the target temperature directly to the
      // compositors.
      current_temperature_ = target_temperature_;
      ApplyTemperatureToAllDisplays(target_temperature_);
      Stop();
      return;
    }

    SetDuration(duration);
    Start();
  }

 private:
  // gfx::Animation:
  void AnimateToState(double state) override {
    state = std::clamp(state, 0.0, 1.0);
    current_temperature_ =
        start_temperature_ + (target_temperature_ - start_temperature_) * state;
  }

  // gfx::AnimationDelegate:
  void AnimationProgressed(const Animation* animation) override {
    DCHECK_EQ(animation, this);

    if (cc::MathUtil::IsWithinEpsilon(current_temperature_,
                                      target_temperature_)) {
      current_temperature_ = target_temperature_;
      Stop();
    }

    ApplyTemperatureToAllDisplays(current_temperature_);
  }

  float start_temperature_ = 0.0f;
  float current_temperature_ = 0.0f;
  float target_temperature_ = 0.0f;
};

NightLightControllerImpl::NightLightControllerImpl()
    : ScheduledFeature(prefs::kNightLightEnabled,
                       prefs::kNightLightScheduleType,
                       prefs::kNightLightCustomStartTime,
                       prefs::kNightLightCustomEndTime),
      temperature_animation_(std::make_unique<ColorTemperatureAnimation>()),
      night_light_metrics_recorder_(
          std::make_unique<NightLightMetricsRecorder>()),
      ambient_temperature_sensor_values_(kMovingAverageWindowSize),
      ambient_temperature_(kNeutralColorTemperatureInKelvin),
      weak_ptr_factory_(this) {
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
  aura::Env::GetInstance()->AddObserver(this);
}

NightLightControllerImpl::~NightLightControllerImpl() {
  aura::Env::GetInstance()->RemoveObserver(this);
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
}

// static
void NightLightControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kNightLightEnabled, false);
  registry->RegisterDoublePref(prefs::kNightLightTemperature,
                               kDefaultColorTemperature);
  const ScheduleType default_schedule_type =
      features::IsAutoNightLightEnabled() ? ScheduleType::kSunsetToSunrise
                                          : ScheduleType::kNone;
  registry->RegisterIntegerPref(prefs::kNightLightScheduleType,
                                static_cast<int>(default_schedule_type));
  registry->RegisterIntegerPref(prefs::kNightLightCustomStartTime,
                                kDefaultStartTimeOffsetMinutes);
  registry->RegisterIntegerPref(prefs::kNightLightCustomEndTime,
                                kDefaultEndTimeOffsetMinutes);
  registry->RegisterBooleanPref(prefs::kAmbientColorEnabled, true);
  registry->RegisterBooleanPref(prefs::kAutoNightLightNotificationDismissed,
                                false);
}

// static
float NightLightControllerImpl::BlueColorScaleFromTemperature(
    float temperature) {
  return 1.0f - temperature;
}

// static
float NightLightControllerImpl::GreenColorScaleFromTemperature(
    float temperature) {
  // If we only tone down the blue scale, the screen will look very green so
  // we also need to tone down the green, but with a less value compared to
  // the blue scale to avoid making things look very red.
  return 1.0f - 0.5f * temperature;
}

// static
float NightLightControllerImpl::RemapAmbientColorTemperature(
    float temperature_in_kelvin) {
  // This function maps sensor input temperatures to other values since we want
  // to avoid extreme color temperatures (e.g: temperatures below 4500 and
  // above 7500 are too extreme.)
  // The following table was created with internal user studies.
  constexpr struct {
    int32_t input_temperature;
    int32_t output_temperature;
  } kTable[] = {{2700, 4500}, {3100, 5000}, {3700, 5300},
                {4200, 5500}, {4800, 5800}, {5300, 6000},
                {6000, 6400}, {7000, 6800}, {8000, 7500}};

  constexpr size_t kTableSize = std::size(kTable);
  // We clamp to a range defined by the minimum possible input value and the
  // maximum. Given that the interval kTable[i].input_temperature,
  // kTable[i+1].input_temperature exclude the upper bound, we clamp it to the
  // last input_temperature element of the table minus 1.
  const float temperature =
      std::clamp<float>(temperature_in_kelvin, kTable[0].input_temperature,
                        kTable[kTableSize - 1].input_temperature - 1);
  for (size_t i = 0; i < kTableSize - 1; i++) {
    if (temperature >= kTable[i].input_temperature &&
        temperature < kTable[i + 1].input_temperature) {
      // Lerp between the output_temperature values of i and i + 1;
      const float t =
          (static_cast<float>(temperature) - kTable[i].input_temperature) /
          (kTable[i + 1].input_temperature - kTable[i].input_temperature);
      return static_cast<float>(kTable[i].output_temperature) +
             t * (kTable[i + 1].output_temperature -
                  kTable[i].output_temperature);
    }
  }
  NOTREACHED();
}

// static
gfx::Vector3dF
NightLightControllerImpl::ColorScalesFromRemappedTemperatureInKevin(
    float temperature_in_kelvin) {
  DCHECK_LT(temperature_in_kelvin, kMaxColorTemperatureInKelvin);
  DCHECK_GE(temperature_in_kelvin, kMinColorTemperatureInKelvin);
  // This function computes the scale factors for R, G and B channel that can be
  // used to compute a CTM matrix. For warmer temperatures we expect green and
  // blue to be scaled down, while red will not change. For cooler temperatures
  // we expect blue not to change while green and blue will be scaled down.
  float red = 1.0f;
  float green = 1.0f;
  float blue = 1.0f;

  // The following formulas are computed with a linear regression to model
  // scalar response from temperature to RGB scale factors. The values were
  // obtained with experiments from internal user studies.
  if (temperature_in_kelvin > kNeutralColorTemperatureInKelvin) {
    float temperature_increment =
        (temperature_in_kelvin - kNeutralColorTemperatureInKelvin) /
        (kMaxColorTemperatureInKelvin - kNeutralColorTemperatureInKelvin);
    red = 1.f - temperature_increment * 0.0929f;
    green = 1.f - temperature_increment * 0.0530f;
  } else {
    float temperature_decrement =
        (kNeutralColorTemperatureInKelvin - temperature_in_kelvin) /
        (kNeutralColorTemperatureInKelvin - kMinColorTemperatureInKelvin);
    green = 1.f - temperature_decrement * 0.1211f;
    blue = 1.f - temperature_decrement * 0.2749f;
  }
  return {red, green, blue};
}

float NightLightControllerImpl::GetColorTemperature() const {
  if (active_user_pref_service()) {
    return active_user_pref_service()->GetDouble(prefs::kNightLightTemperature);
  }

  return kDefaultColorTemperature;
}

void NightLightControllerImpl::UpdateAmbientRgbScalingFactors() {
  ambient_rgb_scaling_factors_ =
      NightLightControllerImpl::ColorScalesFromRemappedTemperatureInKevin(
          ambient_temperature_);
}

void NightLightControllerImpl::SetAmbientColorEnabled(bool enabled) {
  if (active_user_pref_service()) {
    active_user_pref_service()->SetBoolean(prefs::kAmbientColorEnabled,
                                           enabled);
  }
}

bool NightLightControllerImpl::GetAmbientColorEnabled() const {
  return features::IsAllowAmbientEQEnabled() && active_user_pref_service() &&
         active_user_pref_service()->GetBoolean(prefs::kAmbientColorEnabled);
}

void NightLightControllerImpl::SetColorTemperature(float temperature) {
  DCHECK_GE(temperature, 0.0f);
  DCHECK_LE(temperature, 1.0f);
  if (active_user_pref_service()) {
    active_user_pref_service()->SetDouble(prefs::kNightLightTemperature,
                                          temperature);
  }
}

void NightLightControllerImpl::Toggle() {
  SetEnabled(!IsNightLightEnabled());
}

void NightLightControllerImpl::OnDidApplyDisplayChanges() {
  ReapplyColorTemperatures();
}

void NightLightControllerImpl::OnHostInitialized(aura::WindowTreeHost* host) {
  // This newly initialized |host| could be of a newly added display, or of a
  // newly created mirroring display (either for mirroring or unified). we need
  // to apply the current temperature immediately without animation.
  ApplyTemperatureToHost(host,
                         IsNightLightEnabled() ? GetColorTemperature() : 0.0f);
}

bool NightLightControllerImpl::IsNightLightEnabled() const {
  return GetEnabled();
}

void NightLightControllerImpl::Close(bool by_user) {
  if (by_user) {
    DisableShowingFutureAutoNightLightNotification();
    UMA_HISTOGRAM_ENUMERATION(kAutoNightLightNotificationStateHistogram,
                              AutoNightLightNotificationState::kClosedByUser);
  }
}

void NightLightControllerImpl::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  auto* shell = Shell::Get();

  DCHECK(!button_index.has_value());
  // Body has been clicked.
  SystemTrayClient* tray_client = shell->system_tray_model()->client();
  auto* session_controller = shell->session_controller();
  if (session_controller->ShouldEnableSettings() && tray_client) {
    tray_client->ShowDisplaySettings();
  }

  UMA_HISTOGRAM_ENUMERATION(kAutoNightLightNotificationStateHistogram,
                            AutoNightLightNotificationState::kBodyClicked);

  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           /*by_user=*/false);
  // Closing the notification with `by_user=false` above should end up calling
  // NightLightControllerImpl::Close() but will not disable showing the
  // notification any further. We must do this explicitly here.
  DisableShowingFutureAutoNightLightNotification();
  DCHECK(UserHasEverDismissedAutoNightLightNotification());
}

void NightLightControllerImpl::AmbientColorChanged(
    const int32_t color_temperature) {
  ambient_temperature_sensor_values_.AddSample(color_temperature);

  // Use the moving average to calculate the remapped_color_temperature instead
  // of using the sensor color temp directly since the sensor data can be noisy.
  const float remapped_color_temperature =
      RemapAmbientColorTemperature(ambient_temperature_sensor_values_.Mean());
  const float temperature_difference =
      remapped_color_temperature - ambient_temperature_;
  const float abs_temperature_difference = std::abs(temperature_difference);
  // We adjust the ambient color temperature only if the difference with
  // the average ambient temperature computed is greater than a threshold to
  // avoid changing it too often which can cause performance issues.
  constexpr float kAmbientColorChangeThreshold = 50.0f;
  if (abs_temperature_difference < kAmbientColorChangeThreshold) {
    return;
  }

  ambient_temperature_ +=
      (temperature_difference / abs_temperature_difference) *
      kAmbientColorChangeThreshold;

  if (GetAmbientColorEnabled()) {
    UpdateAmbientRgbScalingFactors();
    ReapplyColorTemperatures();
  }
}

message_center::Notification*
NightLightControllerImpl::GetAutoNightLightNotificationForTesting() const {
  return message_center::MessageCenter::Get()->FindVisibleNotificationById(
      kNotificationId);
}

bool NightLightControllerImpl::UserHasEverChangedSchedule() const {
  return active_user_pref_service() && active_user_pref_service()->HasPrefPath(
                                           prefs::kNightLightScheduleType);
}

bool NightLightControllerImpl::UserHasEverDismissedAutoNightLightNotification()
    const {
  return active_user_pref_service() &&
         active_user_pref_service()->GetBoolean(
             prefs::kAutoNightLightNotificationDismissed);
}

void NightLightControllerImpl::ShowAutoNightLightNotification() {
  DCHECK(features::IsAutoNightLightEnabled());
  DCHECK(IsNightLightEnabled());
  DCHECK(!UserHasEverDismissedAutoNightLightNotification());
  DCHECK_EQ(ScheduleType::kSunsetToSunrise, GetScheduleType());

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          l10n_util::GetStringUTF16(IDS_ASH_AUTO_NIGHT_LIGHT_NOTIFY_TITLE),
          l10n_util::GetStringUTF16(IDS_ASH_AUTO_NIGHT_LIGHT_NOTIFY_BODY),
          std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
              NotificationCatalogName::kNightLight),
          message_center::RichNotificationData{},
          base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
              weak_ptr_factory_.GetWeakPtr()),
          kUnifiedMenuNightLightIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_priority(message_center::SYSTEM_PRIORITY);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));

  UMA_HISTOGRAM_BOOLEAN(kAutoNightLightNotificationShownHistogram, true);
}

void NightLightControllerImpl::
    DisableShowingFutureAutoNightLightNotification() {
  if (Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    return;
  }

  if (active_user_pref_service()) {
    active_user_pref_service()->SetBoolean(
        prefs::kAutoNightLightNotificationDismissed, true);
  }
}

void NightLightControllerImpl::RefreshDisplaysTemperature(
    float color_temperature) {
  const float new_temperature =
      IsNightLightEnabled() ? color_temperature : 0.0f;
  temperature_animation_->AnimateToNewValue(
      new_temperature, animation_duration_ == AnimationDuration::kShort
                           ? kManualAnimationDuration
                           : kAutomaticAnimationDuration);

  // Reset the animation type back to manual to consume any automatically set
  // animations.
  last_animation_duration_ = animation_duration_;
  animation_duration_ = AnimationDuration::kShort;
  Shell::Get()->UpdateCursorCompositingEnabled();
}

void NightLightControllerImpl::ReapplyColorTemperatures() {
  DCHECK(temperature_animation_);
  const float target_temperature =
      IsNightLightEnabled() ? GetColorTemperature() : 0.0f;
  if (temperature_animation_->is_animating()) {
    // Do not interrupt an on-going animation towards the same target value.
    if (temperature_animation_->target_temperature() == target_temperature) {
      return;
    }

    NOTREACHED();
  }

  ApplyTemperatureToAllDisplays(target_temperature);
}

void NightLightControllerImpl::NotifyStatusChanged() {
  for (auto& observer : observers_) {
    observer.OnNightLightEnabledChanged(IsNightLightEnabled());
  }
}

void NightLightControllerImpl::OnAmbientColorEnabledPrefChanged() {
  DCHECK(active_user_pref_service());
  if (GetAmbientColorEnabled()) {
    UpdateAmbientRgbScalingFactors();
    VerifyAmbientColorCtmSupport();
  }
  ReapplyColorTemperatures();
}

void NightLightControllerImpl::OnColorTemperaturePrefChanged() {
  DCHECK(active_user_pref_service());
  const float color_temperature = GetColorTemperature();
  UMA_HISTOGRAM_EXACT_LINEAR(
      "Ash.NightLight.Temperature", GetTemperatureRange(color_temperature),
      5 /* number of buckets defined in GetTemperatureRange() */);
  RefreshDisplaysTemperature(color_temperature);
}

void NightLightControllerImpl::RefreshFeatureState(RefreshReason reason) {
  bool enabled_state_changed = false;
  if (active_user_pref_service()) {
    const bool enabled = IsNightLightEnabled();
    enabled_state_changed = last_observed_enabled_state_ != enabled;
    last_observed_enabled_state_ = enabled;

    if (enabled_state_changed) {
      VLOG(1) << "Enable state changed. New state: " << enabled << ".";
      UpdateAutoNightLightNotification(reason);
    }
    is_first_user_init_ = false;
  }

  animation_duration_ = reason == RefreshReason::kScheduled
                            ? AnimationDuration::kLong
                            : AnimationDuration::kShort;
  RefreshDisplaysTemperature(GetColorTemperature());

  if (enabled_state_changed) {
    NotifyStatusChanged();
  }
}

const char* NightLightControllerImpl::GetFeatureName() const {
  return "NightLightControllerImpl";
}

void NightLightControllerImpl::InitFeatureForNewActiveUser() {
  last_observed_enabled_state_.reset();
  if (GetAmbientColorEnabled()) {
    UpdateAmbientRgbScalingFactors();
  }
}

void NightLightControllerImpl::ListenForPrefChanges(
    PrefChangeRegistrar& pref_change_registrar) {
  pref_change_registrar.Add(
      prefs::kNightLightTemperature,
      base::BindRepeating(
          &NightLightControllerImpl::OnColorTemperaturePrefChanged,
          base::Unretained(this)));
  pref_change_registrar.Add(
      prefs::kAmbientColorEnabled,
      base::BindRepeating(
          &NightLightControllerImpl::OnAmbientColorEnabledPrefChanged,
          base::Unretained(this)));
}

const char* NightLightControllerImpl::GetScheduleTypeHistogramName() const {
  return "Ash.NightLight.ScheduleType";
}

void NightLightControllerImpl::UpdateAutoNightLightNotification(
    RefreshReason refresh_reason) {
  DCHECK(active_user_pref_service());

  // When there's no valid geolocation, the default sunset/sunrise times are
  // used, which could lead to Auto Night Light turning on briefly until a valid
  // geolocation is received. At that point, the Notification will be stale, and
  // needs to be removed. It doesn't hurt to remove it always, before we update
  // its state. https://crbug.com/1106586.
  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           /*by_user=*/false);

  if (IsNightLightEnabled() && features::IsAutoNightLightEnabled() &&
      GetScheduleType() == ScheduleType::kSunsetToSunrise &&
      (is_first_user_init_ || refresh_reason == RefreshReason::kScheduled) &&
      !UserHasEverChangedSchedule() &&
      !UserHasEverDismissedAutoNightLightNotification()) {
    VLOG(1) << "Auto Night Light is turning on.";
    ShowAutoNightLightNotification();
  }
}

}  // namespace ash

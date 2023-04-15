// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "ash/system/model/system_tray_model.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
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
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

// Defines the states of the Auto Night Light notification as a result of a
// user's interaction with it.
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "AshAutoNightLightNotificationState" in
// src/tools/metrics/histograms/enums.xml.
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

// The color temperature animation frames per second.
constexpr int kNightLightAnimationFrameRate = 15;

// The following are color temperatues in Kelvin.
// The min/max are a reasonable range we can clamp the values to.
constexpr float kMinColorTemperatureInKelvin = 4500;
constexpr float kNeutralColorTemperatureInKelvin = 6500;
constexpr float kMaxColorTemperatureInKelvin = 7500;

class NightLightControllerDelegateImpl
    : public NightLightControllerImpl::Delegate {
 public:
  NightLightControllerDelegateImpl() = default;
  NightLightControllerDelegateImpl(const NightLightControllerDelegateImpl&) =
      delete;
  NightLightControllerDelegateImpl& operator=(
      const NightLightControllerDelegateImpl&) = delete;
  ~NightLightControllerDelegateImpl() override = default;

  // ash::NightLightControllerImpl::Delegate:
  base::Time GetNow() const override { return base::Time::Now(); }
  base::Time GetSunsetTime() const override { return GetSunRiseSet(false); }
  base::Time GetSunriseTime() const override { return GetSunRiseSet(true); }
  bool SetGeoposition(
      const NightLightController::SimpleGeoposition& position) override {
    if (geoposition_ && *geoposition_ == position)
      return false;

    geoposition_ =
        std::make_unique<NightLightController::SimpleGeoposition>(position);
    return true;
  }
  bool HasGeoposition() const override { return !!geoposition_; }

 private:
  // Note that the below computation is intentionally performed every time
  // GetSunsetTime() or GetSunriseTime() is called rather than once whenever we
  // receive a geoposition (which happens at least once a day). This increases
  // the chances of getting accurate values, especially around DST changes.
  base::Time GetSunRiseSet(bool sunrise) const {
    if (!HasGeoposition()) {
      LOG(ERROR) << "Invalid geoposition. Using default time for "
                 << (sunrise ? "sunrise." : "sunset.");
      return sunrise ? TimeOfDay(kDefaultEndTimeOffsetMinutes).ToTimeToday()
                     : TimeOfDay(kDefaultStartTimeOffsetMinutes).ToTimeToday();
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
        TimeOfDay(12 * 60).ToTimeToday().ToDoubleT();
    astro.setTime(midday_today_sec * 1000.0);
    const double sun_rise_set_ms = astro.getSunRiseSet(sunrise);
    return base::Time::FromDoubleT(sun_rise_set_ms / 1000.0);
  }

  std::unique_ptr<NightLightController::SimpleGeoposition> geoposition_;
};

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
// |apply_ambient_temperature| is true.
// If |in_linear_gamma_space| is true, the generated matrix is the one that
// should be applied after gamma correction, and it corresponds to the
// non-linear temperature value for the given |temperature|.
SkM44 MatrixFromTemperature(float temperature,
                            bool in_linear_gamma_space,
                            bool apply_ambient_temperature) {
  if (in_linear_gamma_space)
    temperature =
        NightLightControllerImpl::GetNonLinearTemperature(temperature);

  SkM44 matrix;
  if (temperature != 0.0f) {
    const float blue_scale =
        NightLightControllerImpl::BlueColorScaleFromTemperature(temperature);
    const float green_scale =
        NightLightControllerImpl::GreenColorScaleFromTemperature(
            temperature, in_linear_gamma_space);

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

// Attempts setting one of the given color matrices on the display hardware of
// |display_id| depending on the hardware capability. The matrix
// |linear_gamma_space_matrix| will be applied if the hardware applies the
// CTM in the linear gamma space (i.e. after gamma decoding), whereas the
// matrix |gamma_compressed_matrix| will be applied instead if the hardware
// applies the CTM in the gamma compressed space (i.e. after degamma
// encoding).
// Returns true if the hardware supports this operation and one of the
// matrices was successfully sent to the GPU.
bool AttemptSettingHardwareCtm(int64_t display_id,
                               const SkM44& linear_gamma_space_matrix,
                               const SkM44& gamma_compressed_matrix) {
  for (const auto* snapshot :
       Shell::Get()->display_configurator()->cached_displays()) {
    if (snapshot->display_id() == display_id &&
        snapshot->has_color_correction_matrix()) {
      return Shell::Get()->display_color_manager()->SetDisplayColorMatrix(
          snapshot, snapshot->color_correction_in_linear_space()
                        ? linear_gamma_space_matrix
                        : gamma_compressed_matrix);
    }
  }

  return false;
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

  const SkM44 linear_gamma_space_matrix =
      MatrixFromTemperature(temperature, true, apply_ambient_temperature);
  const SkM44 gamma_compressed_matrix =
      MatrixFromTemperature(temperature, false, apply_ambient_temperature);
  const bool crtc_result = AttemptSettingHardwareCtm(
      display_id, linear_gamma_space_matrix, gamma_compressed_matrix);
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
    : delegate_(std::make_unique<NightLightControllerDelegateImpl>()),
      temperature_animation_(std::make_unique<ColorTemperatureAnimation>()),
      ambient_temperature_(kNeutralColorTemperatureInKelvin),
      weak_ptr_factory_(this) {
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
  aura::Env::GetInstance()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

NightLightControllerImpl::~NightLightControllerImpl() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
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

  // Non-public prefs, only meant to be used by ash.
  registry->RegisterDoublePref(prefs::kNightLightCachedLatitude, 0.0);
  registry->RegisterDoublePref(prefs::kNightLightCachedLongitude, 0.0);
}

// static
float NightLightControllerImpl::BlueColorScaleFromTemperature(
    float temperature) {
  return 1.0f - temperature;
}

// static
float NightLightControllerImpl::GreenColorScaleFromTemperature(
    float temperature,
    bool in_linear_space) {
  // If we only tone down the blue scale, the screen will look very green so
  // we also need to tone down the green, but with a less value compared to
  // the blue scale to avoid making things look very red.
  return 1.0f - (in_linear_space ? 0.7f : 0.5f) * temperature;
}

// static
float NightLightControllerImpl::GetNonLinearTemperature(float temperature) {
  constexpr float kGammaFactor = 1.0f / 2.2f;
  return std::pow(temperature, kGammaFactor);
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
  return 0;
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
  if (active_user_pref_service_)
    return active_user_pref_service_->GetDouble(prefs::kNightLightTemperature);

  return kDefaultColorTemperature;
}

void NightLightControllerImpl::UpdateAmbientRgbScalingFactors() {
  ambient_rgb_scaling_factors_ =
      NightLightControllerImpl::ColorScalesFromRemappedTemperatureInKevin(
          ambient_temperature_);
}

NightLightController::ScheduleType NightLightControllerImpl::GetScheduleType()
    const {
  if (active_user_pref_service_) {
    return static_cast<ScheduleType>(
        active_user_pref_service_->GetInteger(prefs::kNightLightScheduleType));
  }

  return ScheduleType::kNone;
}

TimeOfDay NightLightControllerImpl::GetCustomStartTime() const {
  if (active_user_pref_service_) {
    return TimeOfDay(active_user_pref_service_->GetInteger(
        prefs::kNightLightCustomStartTime));
  }

  return TimeOfDay(kDefaultStartTimeOffsetMinutes);
}

TimeOfDay NightLightControllerImpl::GetCustomEndTime() const {
  if (active_user_pref_service_) {
    return TimeOfDay(
        active_user_pref_service_->GetInteger(prefs::kNightLightCustomEndTime));
  }

  return TimeOfDay(kDefaultEndTimeOffsetMinutes);
}

void NightLightControllerImpl::SetAmbientColorEnabled(bool enabled) {
  if (active_user_pref_service_)
    active_user_pref_service_->SetBoolean(prefs::kAmbientColorEnabled, enabled);
}

bool NightLightControllerImpl::GetAmbientColorEnabled() const {
  return features::IsAllowAmbientEQEnabled() && active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(prefs::kAmbientColorEnabled);
}

bool NightLightControllerImpl::IsNowWithinSunsetSunrise() const {
  // The times below are all on the same calendar day.
  const base::Time now = delegate_->GetNow();
  return now < delegate_->GetSunriseTime() || now > delegate_->GetSunsetTime();
}

void NightLightControllerImpl::SetEnabled(bool enabled,
                                          AnimationDuration animation_type) {
  if (active_user_pref_service_) {
    animation_duration_ = animation_type;
    active_user_pref_service_->SetBoolean(prefs::kNightLightEnabled, enabled);
  }
}

void NightLightControllerImpl::SetColorTemperature(float temperature) {
  DCHECK_GE(temperature, 0.0f);
  DCHECK_LE(temperature, 1.0f);
  if (active_user_pref_service_) {
    active_user_pref_service_->SetDouble(prefs::kNightLightTemperature,
                                         temperature);
  }
}

void NightLightControllerImpl::SetScheduleType(ScheduleType type) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(prefs::kNightLightScheduleType,
                                          static_cast<int>(type));
  }
}

void NightLightControllerImpl::SetCustomStartTime(TimeOfDay start_time) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(
        prefs::kNightLightCustomStartTime,
        start_time.offset_minutes_from_zero_hour());
  }
}

void NightLightControllerImpl::SetCustomEndTime(TimeOfDay end_time) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(
        prefs::kNightLightCustomEndTime,
        end_time.offset_minutes_from_zero_hour());
  }
}

void NightLightControllerImpl::Toggle() {
  SetEnabled(!GetEnabled(), AnimationDuration::kShort);
}

void NightLightControllerImpl::OnDisplayConfigurationChanged() {
  ReapplyColorTemperatures();
}

void NightLightControllerImpl::OnHostInitialized(aura::WindowTreeHost* host) {
  // This newly initialized |host| could be of a newly added display, or of a
  // newly created mirroring display (either for mirroring or unified). we need
  // to apply the current temperature immediately without animation.
  ApplyTemperatureToHost(host, GetEnabled() ? GetColorTemperature() : 0.0f);
}

void NightLightControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_service == active_user_pref_service_)
    return;

  // TODO(afakhry|yjliu): Remove this VLOG when https://crbug.com/1015474 is
  // fixed.
  auto vlog_helper = [](const PrefService* pref_service) -> std::string {
    if (!pref_service)
      return "None";
    return base::StringPrintf(
        "{State %s, Schedule Type: %d}",
        pref_service->GetBoolean(prefs::kNightLightEnabled) ? "enabled"
                                                            : "disabled",
        pref_service->GetInteger(prefs::kNightLightScheduleType));
  };
  VLOG(1) << "Switching user pref service from "
          << vlog_helper(active_user_pref_service_) << " to "
          << vlog_helper(pref_service) << ".";

  // Initial login and user switching in multi profiles.
  active_user_pref_service_ = pref_service;
  InitFromUserPrefs();
}

void NightLightControllerImpl::SetCurrentGeoposition(
    const SimpleGeoposition& position) {
  VLOG(1) << "Received new geoposition.";

  is_current_geoposition_from_cache_ = false;
  StoreCachedGeoposition(position);

  const base::Time previous_sunset = delegate_->GetSunsetTime();
  const base::Time previous_sunrise = delegate_->GetSunriseTime();

  if (!delegate_->SetGeoposition(position)) {
    VLOG(1) << "Not refreshing since geoposition hasn't changed";
    return;
  }

  // If the schedule type is sunset to sunrise or custom, a potential change in
  // the geoposition might mean timezone change as well as a change in the start
  // and end times. In these cases, we must trigger a refresh.
  if (GetScheduleType() == ScheduleType::kNone)
    return;

  // We only keep manual toggles if the change in geoposition results in an hour
  // or more in either sunset or sunrise times. A one-hour threshold is used
  // here as an indication of a possible timezone change, and this case, manual
  // toggles should be ignored.
  constexpr base::TimeDelta kOneHourDuration = base::Hours(1);
  const bool keep_manual_toggles_during_schedules =
      (delegate_->GetSunsetTime() - previous_sunset).magnitude() <
          kOneHourDuration &&
      (delegate_->GetSunriseTime() - previous_sunrise).magnitude() <
          kOneHourDuration;

  Refresh(/*did_schedule_change=*/true, keep_manual_toggles_during_schedules);
}

bool NightLightControllerImpl::GetEnabled() const {
  return active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(prefs::kNightLightEnabled);
}

void NightLightControllerImpl::SuspendDone(base::TimeDelta sleep_duration) {
  // Time changes while the device is suspended. We need to refresh the schedule
  // upon device resume to know what the status should be now.
  Refresh(/*did_schedule_change=*/true,
          /*keep_manual_toggles_during_schedules=*/true);
}

void NightLightControllerImpl::Close(bool by_user) {
  if (by_user) {
    DisableShowingFutureAutoNightLightNotification();
    UMA_HISTOGRAM_ENUMERATION(kAutoNightLightNotificationStateHistogram,
                              AutoNightLightNotificationState::kClosedByUser);
  }
}

void NightLightControllerImpl::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  auto* shell = Shell::Get();

  DCHECK(!button_index.has_value());
  // Body has been clicked.
  SystemTrayClient* tray_client = shell->system_tray_model()->client();
  auto* session_controller = shell->session_controller();
  if (session_controller->ShouldEnableSettings() && tray_client)
    tray_client->ShowDisplaySettings();

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
  const float remapped_color_temperature =
      RemapAmbientColorTemperature(color_temperature);
  const float temperature_difference =
      remapped_color_temperature - ambient_temperature_;
  const float abs_temperature_difference = std::abs(temperature_difference);
  // We adjust the ambient color temperature only if the difference with
  // the last ambient temperature computed is greated than a threshold to
  // avoid changing it too often when the powerd readings are noisy.
  constexpr float kAmbientColorChangeThreshold = 100.0f;
  if (abs_temperature_difference < kAmbientColorChangeThreshold)
    return;

  ambient_temperature_ +=
      (temperature_difference / abs_temperature_difference) *
      kAmbientColorChangeThreshold;

  if (GetAmbientColorEnabled()) {
    UpdateAmbientRgbScalingFactors();
    ReapplyColorTemperatures();
  }
}

void NightLightControllerImpl::SetDelegateForTesting(
    std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

message_center::Notification*
NightLightControllerImpl::GetAutoNightLightNotificationForTesting() const {
  return message_center::MessageCenter::Get()->FindVisibleNotificationById(
      kNotificationId);
}

bool NightLightControllerImpl::MaybeRestoreSchedule() {
  DCHECK(active_user_pref_service_);
  DCHECK_NE(GetScheduleType(), ScheduleType::kNone);

  auto iter = per_user_schedule_target_state_.find(active_user_pref_service_);
  if (iter == per_user_schedule_target_state_.end())
    return false;

  ScheduleTargetState& target_state = iter->second;
  // It may be that the device was suspended for a very long time that the
  // target time is no longer valid.
  if (target_state.target_time <= delegate_->GetNow())
    return false;

  VLOG(1) << "Restoring a previous schedule.";
  DCHECK_NE(GetEnabled(), target_state.target_status);
  ScheduleNextToggle(target_state.target_time - delegate_->GetNow());
  return true;
}

bool NightLightControllerImpl::UserHasEverChangedSchedule() const {
  return active_user_pref_service_ &&
         active_user_pref_service_->HasPrefPath(prefs::kNightLightScheduleType);
}

bool NightLightControllerImpl::UserHasEverDismissedAutoNightLightNotification()
    const {
  return active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(
             prefs::kAutoNightLightNotificationDismissed);
}

void NightLightControllerImpl::ShowAutoNightLightNotification() {
  DCHECK(features::IsAutoNightLightEnabled());
  DCHECK(GetEnabled());
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
  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    return;

  if (active_user_pref_service_) {
    active_user_pref_service_->SetBoolean(
        prefs::kAutoNightLightNotificationDismissed, true);
  }
}

void NightLightControllerImpl::LoadCachedGeopositionIfNeeded() {
  DCHECK(active_user_pref_service_);

  // Even if there is a geoposition, but it's coming from a previously cached
  // value, switching users should load the currently saved values for the
  // new user. This is to keep users' prefs completely separate. We only ignore
  // the cached values once we have a valid non-cached geoposition from any
  // user in the same session.
  if (delegate_->HasGeoposition() && !is_current_geoposition_from_cache_)
    return;

  if (!active_user_pref_service_->HasPrefPath(
          prefs::kNightLightCachedLatitude) ||
      !active_user_pref_service_->HasPrefPath(
          prefs::kNightLightCachedLongitude)) {
    VLOG(1) << "No valid current geoposition and no valid cached geoposition"
               " are available. Will use default times for sunset / sunrise.";
    return;
  }

  VLOG(1) << "Temporarily using a previously cached geoposition.";
  delegate_->SetGeoposition(SimpleGeoposition{
      active_user_pref_service_->GetDouble(prefs::kNightLightCachedLatitude),
      active_user_pref_service_->GetDouble(prefs::kNightLightCachedLongitude)});
  is_current_geoposition_from_cache_ = true;
}

void NightLightControllerImpl::StoreCachedGeoposition(
    const SimpleGeoposition& position) {
  const SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  for (const auto& user_session : session_controller->GetUserSessions()) {
    PrefService* pref_service = session_controller->GetUserPrefServiceForUser(
        user_session->user_info.account_id);
    if (!pref_service)
      continue;

    pref_service->SetDouble(prefs::kNightLightCachedLatitude,
                            position.latitude);
    pref_service->SetDouble(prefs::kNightLightCachedLongitude,
                            position.longitude);
  }
}

void NightLightControllerImpl::RefreshDisplaysTemperature(
    float color_temperature) {
  const float new_temperature = GetEnabled() ? color_temperature : 0.0f;
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
  const float target_temperature = GetEnabled() ? GetColorTemperature() : 0.0f;
  if (temperature_animation_->is_animating()) {
    // Do not interrupt an on-going animation towards the same target value.
    if (temperature_animation_->target_temperature() == target_temperature)
      return;

    NOTREACHED();
    temperature_animation_->Stop();
  }

  ApplyTemperatureToAllDisplays(target_temperature);
}

void NightLightControllerImpl::StartWatchingPrefsChanges() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs::kNightLightEnabled,
      base::BindRepeating(&NightLightControllerImpl::OnEnabledPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kNightLightTemperature,
      base::BindRepeating(
          &NightLightControllerImpl::OnColorTemperaturePrefChanged,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kNightLightScheduleType,
      base::BindRepeating(&NightLightControllerImpl::OnScheduleTypePrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kNightLightCustomStartTime,
      base::BindRepeating(
          &NightLightControllerImpl::OnCustomSchedulePrefsChanged,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kNightLightCustomEndTime,
      base::BindRepeating(
          &NightLightControllerImpl::OnCustomSchedulePrefsChanged,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAmbientColorEnabled,
      base::BindRepeating(
          &NightLightControllerImpl::OnAmbientColorEnabledPrefChanged,
          base::Unretained(this)));

  // Note: No need to observe changes in the cached latitude/longitude since
  // they're only accessed here in ash. We only load them when the active user
  // changes, and store them whenever we receive an updated geoposition.
}

void NightLightControllerImpl::InitFromUserPrefs() {
  StartWatchingPrefsChanges();
  LoadCachedGeopositionIfNeeded();
  if (GetAmbientColorEnabled())
    UpdateAmbientRgbScalingFactors();
  Refresh(/*did_schedule_change=*/true,
          /*keep_manual_toggles_during_schedules=*/true);
  NotifyStatusChanged();
  NotifyClientWithScheduleChange();
  is_first_user_init_ = false;
}

void NightLightControllerImpl::NotifyStatusChanged() {
  for (auto& observer : observers_)
    observer.OnNightLightEnabledChanged(GetEnabled());
}

void NightLightControllerImpl::NotifyClientWithScheduleChange() {
  for (auto& observer : observers_)
    observer.OnScheduleTypeChanged(GetScheduleType());
}

void NightLightControllerImpl::OnEnabledPrefChanged() {
  const bool enabled = GetEnabled();
  VLOG(1) << "Enable state changed. New state: " << enabled << ".";
  DCHECK(active_user_pref_service_);

  // When there's no valid geolocation, the default sunset/sunrise times are
  // used, which could lead to Auto Night Light turning on briefly until a valid
  // geolocation is received. At that point, the Notification will be stale, and
  // needs to be removed. It doesn't hurt to remove it always, before we update
  // its state. https://crbug.com/1106586.
  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           /*by_user=*/false);

  if (enabled && features::IsAutoNightLightEnabled() &&
      GetScheduleType() == ScheduleType::kSunsetToSunrise &&
      (is_first_user_init_ ||
       animation_duration_ == AnimationDuration::kLong) &&
      !UserHasEverChangedSchedule() &&
      !UserHasEverDismissedAutoNightLightNotification()) {
    VLOG(1) << "Auto Night Light is turning on.";
    ShowAutoNightLightNotification();
  }

  Refresh(/*did_schedule_change=*/false,
          /*keep_manual_toggles_during_schedules=*/false);
  NotifyStatusChanged();
}

void NightLightControllerImpl::OnAmbientColorEnabledPrefChanged() {
  DCHECK(active_user_pref_service_);
  if (GetAmbientColorEnabled()) {
    UpdateAmbientRgbScalingFactors();
    VerifyAmbientColorCtmSupport();
  }
  ReapplyColorTemperatures();
}

void NightLightControllerImpl::OnColorTemperaturePrefChanged() {
  DCHECK(active_user_pref_service_);
  const float color_temperature = GetColorTemperature();
  UMA_HISTOGRAM_EXACT_LINEAR(
      "Ash.NightLight.Temperature", GetTemperatureRange(color_temperature),
      5 /* number of buckets defined in GetTemperatureRange() */);
  RefreshDisplaysTemperature(color_temperature);
}

void NightLightControllerImpl::OnScheduleTypePrefChanged() {
  VLOG(1) << "Schedule type changed. New type: "
          << static_cast<int>(GetScheduleType()) << ".";
  DCHECK(active_user_pref_service_);
  NotifyClientWithScheduleChange();
  Refresh(/*did_schedule_change=*/true,
          /*keep_manual_toggles_during_schedules=*/false);

  UMA_HISTOGRAM_ENUMERATION("Ash.NightLight.ScheduleType", GetScheduleType());
}

void NightLightControllerImpl::OnCustomSchedulePrefsChanged() {
  DCHECK(active_user_pref_service_);
  Refresh(/*did_schedule_change=*/true,
          /*keep_manual_toggles_during_schedules=*/false);
}

void NightLightControllerImpl::Refresh(
    bool did_schedule_change,
    bool keep_manual_toggles_during_schedules) {
  switch (GetScheduleType()) {
    case ScheduleType::kNone:
      timer_.Stop();
      RefreshDisplaysTemperature(GetColorTemperature());
      return;

    case ScheduleType::kSunsetToSunrise:
      RefreshScheduleTimer(delegate_->GetSunsetTime(),
                           delegate_->GetSunriseTime(), did_schedule_change,
                           keep_manual_toggles_during_schedules);
      return;

    case ScheduleType::kCustom:
      RefreshScheduleTimer(
          GetCustomStartTime().ToTimeToday(), GetCustomEndTime().ToTimeToday(),
          did_schedule_change, keep_manual_toggles_during_schedules);
      return;
  }
}

void NightLightControllerImpl::RefreshScheduleTimer(
    base::Time start_time,
    base::Time end_time,
    bool did_schedule_change,
    bool keep_manual_toggles_during_schedules) {
  if (GetScheduleType() == ScheduleType::kNone) {
    NOTREACHED();
    timer_.Stop();
    return;
  }

  if (keep_manual_toggles_during_schedules && MaybeRestoreSchedule()) {
    RefreshDisplaysTemperature(GetColorTemperature());
    return;
  }

  // NOTE: Users can set any weird combinations.
  const base::Time now = delegate_->GetNow();
  if (end_time <= start_time) {
    // Example:
    // Start: 9:00 PM, End: 6:00 AM.
    //
    //       6:00                21:00
    // <----- + ------------------ + ----->
    //        |                    |
    //       end                 start
    //
    // Note that the above times are times of day (today). It is important to
    // know where "now" is with respect to these times to decide how to adjust
    // them.
    if (end_time >= now) {
      // If the end time (today) is greater than the time now, this means "now"
      // is within the NightLight schedule, and the start time is actually
      // yesterday. The above timeline is interpreted as:
      //
      //   21:00 (-1day)              6:00
      // <----- + ----------- + ------ + ----->
      //        |             |        |
      //      start          now      end
      //
      start_time -= base::Days(1);
    } else {
      // Two possibilities here:
      // - Either "now" is greater than the end time, but less than start time.
      //   This means NightLight is outside the schedule, waiting for the next
      //   start time. The end time is actually a day later.
      // - Or "now" is greater than both the start and end times. This means
      //   NightLight is within the schedule, waiting to turn off at the next
      //   end time, which is also a day later.
      end_time += base::Days(1);
    }
  }

  DCHECK_GE(end_time, start_time);

  // The target status that we need to set NightLight to now if a change of
  // status is needed immediately.
  bool enable_now = false;

  // Where are we now with respect to the start and end times?
  if (now < start_time) {
    // Example:
    // Start: 6:00 PM today, End: 6:00 AM tomorrow, Now: 4:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //       now          start          end
    //
    // In this case, we need to disable NightLight immediately if it's enabled.
    enable_now = false;
  } else if (now >= start_time && now < end_time) {
    // Example:
    // Start: 6:00 PM today, End: 6:00 AM tomorrow, Now: 11:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //      start          now           end
    //
    // Start NightLight right away. Our future start time is a day later than
    // its current value.
    enable_now = true;
    start_time += base::Days(1);
  } else {  // now >= end_time.
    // Example:
    // Start: 6:00 PM today, End: 10:00 PM today, Now: 11:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //      start          end           now
    //
    // In this case, our future start and end times are a day later from their
    // current values. NightLight needs to be ended immediately if it's already
    // enabled.
    enable_now = false;
    start_time += base::Days(1);
    end_time += base::Days(1);
  }

  // After the above processing, the start and end time are all in the future.
  DCHECK_GE(start_time, now);
  DCHECK_GE(end_time, now);

  if (did_schedule_change && enable_now != GetEnabled()) {
    // If the change in the schedule introduces a change in the status, then
    // calling SetEnabled() is all we need, since it will trigger a change in
    // the user prefs to which we will respond by calling Refresh(). This will
    // end up in this function again, adjusting all the needed schedules.
    SetEnabled(enable_now, AnimationDuration::kShort);
    return;
  }

  // We reach here in one of the following conditions:
  // 1) If schedule changes don't result in changes in the status, we need to
  // explicitly update the timer to re-schedule the next toggle to account for
  // any changes.
  // 2) The user has just manually toggled the status of NightLight either from
  // the System Menu or System Settings. In this case, we respect the user
  // wish and maintain the current status that they desire, but we schedule the
  // status to be toggled according to the time that corresponds with the
  // opposite status of the current one.
  ScheduleNextToggle(GetEnabled() ? end_time - now : start_time - now);
  RefreshDisplaysTemperature(GetColorTemperature());
}

void NightLightControllerImpl::ScheduleNextToggle(base::TimeDelta delay) {
  DCHECK(active_user_pref_service_);

  const bool new_status = !GetEnabled();
  const base::Time target_time = delegate_->GetNow() + delay;

  per_user_schedule_target_state_[active_user_pref_service_] =
      ScheduleTargetState{target_time, new_status};

  VLOG(1) << "Setting Night Light to toggle to "
          << (new_status ? "enabled" : "disabled") << " at "
          << base::TimeFormatTimeOfDay(target_time);
  timer_.Start(FROM_HERE, delay,
               base::BindOnce(&NightLightControllerImpl::SetEnabled,
                              base::Unretained(this), new_status,
                              AnimationDuration::kLong));
}

}  // namespace ash

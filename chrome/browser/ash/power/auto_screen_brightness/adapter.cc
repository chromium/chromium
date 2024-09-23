// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/adapter.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/power/auto_screen_brightness/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

namespace {

constexpr double kTol = 1e-10;

constexpr double kMaxBrightnessAdjustment = 100;
constexpr double kMinBrightnessAdjustment = 1;
constexpr size_t kNumBrightnessAdjustmentBuckets = 10;

const char* BrightnessChangeCauseToString(
    Adapter::BrightnessChangeCause cause) {
  switch (cause) {
    case Adapter::BrightnessChangeCause::kInitialAlsReceived:
      return "InitialAlsReceived";
    case Adapter::BrightnessChangeCause::kBrightneningThresholdExceeded:
      return "BrightneningThresholdExceeded";
    case Adapter::BrightnessChangeCause::kDarkeningThresholdExceeded:
      return "DarkeningThresholdExceeded";
    case Adapter::BrightnessChangeCause::kUpdateAfterLidReopen:
      return "UpdateAfterLidReopen";
    // |kImmediateBrightneningThresholdExceeded| and
    // |kImmediateDarkeningThresholdExceeded| are deprecated, and shouldn't show
    // up.
    case Adapter::BrightnessChangeCause::
        kImmediateBrightneningThresholdExceeded:
    case Adapter::BrightnessChangeCause::kImmediateDarkeningThresholdExceeded:
      return "UnexpectedImmediateTransition";
  }
  return "Unknown";
}

// Multiplies input |x| by a factor of 100 and round to the nearest int.
int ScaleAndConvertToInt(double x) {
  return static_cast<int>(x * 100 + 0.5);
}

// Returns the bucket number from |value| following the exponential bucketing
// scheme. Each bucket is inclusive from the left and exclusive from the right.
int ExponentialBucketing(double value) {
  static const double kExponentialBrightnessAdjustmentStepSize =
      std::log(kMaxBrightnessAdjustment / kMinBrightnessAdjustment) /
      (kNumBrightnessAdjustmentBuckets - 1);
  DCHECK_LE(kMinBrightnessAdjustment, value);
  DCHECK_LE(value, kMaxBrightnessAdjustment);
  return static_cast<int>(std::log(value / kMinBrightnessAdjustment) /
                          kExponentialBrightnessAdjustmentStepSize);
}

}  // namespace

Adapter::Params::Params() = default;

Adapter::AdapterDecision::AdapterDecision() = default;

Adapter::AdapterDecision::AdapterDecision(const AdapterDecision& decision) =
    default;

Adapter::AdapterDecision& Adapter::AdapterDecision::operator=(
    const AdapterDecision& decision) = default;

Adapter::Adapter(Profile* profile,
                 AlsReader* als_reader,
                 BrightnessMonitor* brightness_monitor,
                 Modeller* modeller,
                 ModelConfigLoader* model_config_loader,
                 MetricsReporter* metrics_reporter)
    : Adapter(profile,
              als_reader,
              brightness_monitor,
              modeller,
              model_config_loader,
              metrics_reporter,
              base::DefaultTickClock::GetInstance()) {}

Adapter::~Adapter() = default;

void Adapter::Init() {
  // Deferred to Init() because it can result in a virtual method being called.
  power_manager_client_observation_.Observe(
      chromeos::PowerManagerClient::Get());
}

void Adapter::OnAmbientLightUpdated(int lux) {
  const base::TimeTicks now = tick_clock_->NowTicks();

  // Ambient light data is only used when adapter is initialized to success.
  // |log_als_values_| may not be available to use when adapter is being
  // initialized.
  if (adapter_status_ != Status::kSuccess)
    return;

  DCHECK(log_als_values_);

  // We may have no prior lid event received, if lux value is > 0, then it's
  // safe to assume the lid is open.
  if (!is_lid_closed_.has_value())
    is_lid_closed_ = lux == 0;

  // We do not record ALS value if lid is closed.
  if (*is_lid_closed_) {
    if (!lid_closed_message_reported_) {
      VLOG(1) << "ABAdapter ALS ignored while lid-closed";
      lid_closed_message_reported_ = true;
    }
    return;
  }

  if (now - lid_reopen_time_ < lid_open_delay_time_) {
    return;
  }

  log_als_values_->SaveToBuffer({ConvertToLog(lux), now});

  const AdapterDecision& decision = CanAdjustBrightness(now);

  if (decision.no_brightness_change_cause)
    return;

  DCHECK(decision.brightness_change_cause);
  DCHECK(decision.log_als_avg_stddev);

  AdjustBrightness(*decision.brightness_change_cause,
                   decision.log_als_avg_stddev->avg);
}

void Adapter::OnAlsReaderInitialized(AlsReader::AlsInitStatus status) {
  DCHECK(!als_init_status_);

  als_init_status_ = status;
  als_init_time_ = tick_clock_->NowTicks();
  UpdateStatus();
}

void Adapter::OnBrightnessMonitorInitialized(bool success) {
  DCHECK(!brightness_monitor_success_.has_value());

  brightness_monitor_success_ = success;
  UpdateStatus();
}

void Adapter::OnUserBrightnessChanged(double old_brightness_percent,
                                      double new_brightness_percent) {
  const auto first_recent_user_brightness_request_time =
      first_recent_user_brightness_request_time_;
  const auto decision_at_first_recent_user_brightness_request =
      decision_at_first_recent_user_brightness_request_;

  first_recent_user_brightness_request_time_ = std::nullopt;
  decision_at_first_recent_user_brightness_request_ = std::nullopt;

  // We skip this notification if adapter hasn't been initialised because its
  // |params_| may change. We need to log even if adapter is initialized to
  // disabled.
  if (adapter_status_ == Status::kInitializing) {
    return;
  }

  // |latest_brightness_change_time_|, |current_brightness_|,
  // |average_log_ambient_lux_| and thresholds are only needed if adapter is
  // |kSuccess|.
  if (adapter_status_ == Status::kSuccess) {
    if (!decision_at_first_recent_user_brightness_request) {
      // This should not happen frequently.
      UMA_HISTOGRAM_BOOLEAN(
          "AutoScreenBrightness.MissingPriorUserBrightnessRequest", true);
      return;
    }
    DCHECK(first_recent_user_brightness_request_time);
    LogAdapterDecision(*first_recent_user_brightness_request_time,
                       *decision_at_first_recent_user_brightness_request,
                       old_brightness_percent, new_brightness_percent);

    const std::optional<AlsAvgStdDev> log_als_avg_stddev =
        decision_at_first_recent_user_brightness_request->log_als_avg_stddev;

    const std::string log_als =
        log_als_avg_stddev ? base::StringPrintf("%.4f", log_als_avg_stddev->avg)
                           : "";
    OnBrightnessChanged(
        *first_recent_user_brightness_request_time, new_brightness_percent,
        log_als_avg_stddev ? std::optional<double>(log_als_avg_stddev->avg)
                           : std::nullopt);
  }

  if (!metrics_reporter_)
    return;

  metrics_reporter_->OnUserBrightnessChangeRequested();
}

void Adapter::OnUserBrightnessChangeRequested() {
  const base::TimeTicks now = tick_clock_->NowTicks();
  // We skip this notification if adapter hasn't been initialised (because its
  // |params_| may change), or, if adapter is disabled (because adapter won't
  // change brightness anyway).
  if (adapter_status_ != Status::kSuccess) {
    // Set |first_recent_user_brightness_request_time_| if not already set, so
    // that it won't be reset.
    if (!first_recent_user_brightness_request_time_)
      first_recent_user_brightness_request_time_ = now;
    return;
  }

  if (!first_recent_user_brightness_request_time_) {
    DCHECK(log_als_values_);
    // Check what model would say and also get latest AlsAvgStdDev.
    decision_at_first_recent_user_brightness_request_ =
        CanAdjustBrightness(now);
    first_recent_user_brightness_request_time_ = now;
    model_iteration_count_at_user_brightness_change_ = model_.iteration_count;
  }

  if (!adapter_disabled_by_user_adjustment_) {
    // It's possible a new curve arrives after a user brighntess change disables
    // the adapter, in that case we don't want to reset the |new_model_arrived_|
    // because we could use this model after the adapter is re-enabled.
    new_model_arrived_ = false;
  }

  if (params_.user_adjustment_effect != UserAdjustmentEffect::kContinueAuto) {
    // Adapter will stop making brightness adjustment until suspend/resume or
    // when browser restarts.
    adapter_disabled_by_user_adjustment_ = true;
  }
}

void Adapter::OnModelTrained(const MonotoneCubicSpline& brightness_curve) {
  // It's ok to record brightness curve even when adapter is not completely
  // initialized. But we stop recording curves if we know adapter is disabled.
  if (adapter_status_ == Status::kDisabled)
    return;

  model_.personal_curve = brightness_curve;
  ++model_.iteration_count;
  new_model_arrived_ = true;
}

void Adapter::OnModelInitialized(const Model& model) {
  DCHECK(!model_initialized_);

  model_initialized_ = true;
  model_ = model;
  new_model_arrived_ = true;

  UpdateStatus();
}

void Adapter::OnModelConfigLoaded(std::optional<ModelConfig> model_config) {
  DCHECK(!enabled_by_model_configs_.has_value());

  enabled_by_model_configs_ = model_config.has_value();

  if (enabled_by_model_configs_.value()) {
    InitParams(model_config.value());
  }

  UpdateStatus();
}

void Adapter::PowerManagerBecameAvailable(bool service_is_ready) {
  power_manager_service_available_ = service_is_ready;
  UpdateStatus();
}

void Adapter::SuspendDone(base::TimeDelta /* sleep_duration */) {
  // We skip this notification if adapter hasn't been initialised (because its
  // |params_| may change), or, if adapter is disabled (because adapter won't
  // change brightness anyway).
  if (adapter_status_ != Status::kSuccess)
    return;

  if (params_.user_adjustment_effect == UserAdjustmentEffect::kPauseAuto)
    adapter_disabled_by_user_adjustment_ = false;
}

void Adapter::LidEventReceived(chromeos::PowerManagerClient::LidState state,
                               base::TimeTicks /* timestamp */) {
  is_lid_closed_ = state == chromeos::PowerManagerClient::LidState::CLOSED;
  if (!*is_lid_closed_) {
    lid_reopen_time_ = tick_clock_->NowTicks();
    lid_closed_message_reported_ = false;
    return;
  }

  if (log_als_values_) {
    log_als_values_->ClearBuffer();
  }
}

Adapter::Status Adapter::GetStatusForTesting() const {
  return adapter_status_;
}

bool Adapter::IsAppliedForTesting() const {
  return (adapter_status_ == Status::kSuccess &&
          !adapter_disabled_by_user_adjustment_);
}

std::optional<MonotoneCubicSpline> Adapter::GetGlobalCurveForTesting() const {
  return model_.global_curve;
}

std::optional<MonotoneCubicSpline> Adapter::GetPersonalCurveForTesting() const {
  return model_.personal_curve;
}

std::optional<AlsAvgStdDev> Adapter::GetAverageAmbientWithStdDevForTesting(
    base::TimeTicks now) {
  DCHECK(log_als_values_);
  return log_als_values_->AverageAmbientWithStdDev(now);
}

double Adapter::GetBrighteningThresholdForTesting() const {
  return *brightening_threshold_;
}

double Adapter::GetDarkeningThresholdForTesting() const {
  return *darkening_threshold_;
}

std::optional<double> Adapter::GetCurrentAvgLogAlsForTesting() const {
  return average_log_ambient_lux_;
}

std::unique_ptr<Adapter> Adapter::CreateForTesting(
    Profile* profile,
    AlsReader* als_reader,
    BrightnessMonitor* brightness_monitor,
    Modeller* modeller,
    ModelConfigLoader* model_config_loader,
    MetricsReporter* metrics_reporter,
    const base::TickClock* tick_clock) {
  return base::WrapUnique(new Adapter(profile, als_reader, brightness_monitor,
                                      modeller, model_config_loader,
                                      metrics_reporter, tick_clock));
}

Adapter::Adapter(Profile* profile,
                 AlsReader* als_reader,
                 BrightnessMonitor* brightness_monitor,
                 Modeller* modeller,
                 ModelConfigLoader* model_config_loader,
                 MetricsReporter* metrics_reporter,
                 const base::TickClock* tick_clock)
    : profile_(profile),
      metrics_reporter_(metrics_reporter),
      tick_clock_(tick_clock) {
  DCHECK(profile);
  DCHECK(als_reader);
  DCHECK(brightness_monitor);
  DCHECK(modeller);
  DCHECK(model_config_loader);

  als_reader_observation_.Observe(als_reader);
  brightness_monitor_observation_.Observe(brightness_monitor);
  modeller_observation_.Observe(modeller);
  model_config_loader_observation_.Observe(model_config_loader);

  const int lid_open_delay_time_seconds = GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "lid_open_delay_time_seconds",
      lid_open_delay_time_.InSeconds());

  if (lid_open_delay_time_seconds > 0) {
    lid_open_delay_time_ = base::Seconds(lid_open_delay_time_seconds);
  }
}

void Adapter::InitParams(const ModelConfig& model_config) {
  params_.metrics_key = model_config.metrics_key;
  if (!base::FeatureList::IsEnabled(features::kAutoScreenBrightness) ||
      !model_config.enabled) {
    enabled_by_model_configs_ = false;
    return;
  }

  params_.brightening_log_lux_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "brightening_log_lux_threshold",
      params_.brightening_log_lux_threshold);

  params_.darkening_log_lux_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "darkening_log_lux_threshold",
      params_.darkening_log_lux_threshold);

  params_.stabilization_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "stabilization_threshold",
      params_.stabilization_threshold);

  params_.auto_brightness_als_horizon =
      base::Seconds(model_config.auto_brightness_als_horizon_seconds);

  log_als_values_ = std::make_unique<AmbientLightSampleBuffer>(
      params_.auto_brightness_als_horizon);

  const int user_adjustment_effect_as_int = GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "user_adjustment_effect",
      static_cast<int>(params_.user_adjustment_effect));
  if (user_adjustment_effect_as_int < 0 || user_adjustment_effect_as_int > 2) {
    enabled_by_model_configs_ = false;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }
  params_.user_adjustment_effect =
      static_cast<UserAdjustmentEffect>(user_adjustment_effect_as_int);

  UMA_HISTOGRAM_ENUMERATION("AutoScreenBrightness.UserAdjustmentEffect",
                            params_.user_adjustment_effect);
}

void Adapter::UpdateStatus() {
  if (adapter_status_ != Status::kInitializing)
    return;

  if (!als_init_status_)
    return;

  const bool als_success =
      *als_init_status_ == AlsReader::AlsInitStatus::kSuccess;
  if (!als_success) {
    adapter_status_ = Status::kDisabled;
    SetMetricsReporterDeviceClass();
    return;
  }

  if (!brightness_monitor_success_.has_value())
    return;

  if (!*brightness_monitor_success_) {
    adapter_status_ = Status::kDisabled;
    SetMetricsReporterDeviceClass();
    return;
  }

  if (!model_initialized_)
    return;

  if (!model_.global_curve) {
    adapter_status_ = Status::kDisabled;
    SetMetricsReporterDeviceClass();
    return;
  }

  if (!power_manager_service_available_.has_value())
    return;

  if (!*power_manager_service_available_) {
    adapter_status_ = Status::kDisabled;
    SetMetricsReporterDeviceClass();
    return;
  }

  if (!enabled_by_model_configs_.has_value())
    return;

  if (!enabled_by_model_configs_.value()) {
    adapter_status_ = Status::kDisabled;
    SetMetricsReporterDeviceClass();
    return;
  }

  adapter_status_ = Status::kSuccess;
  SetMetricsReporterDeviceClass();
}

void Adapter::SetMetricsReporterDeviceClass() {
  if (!metrics_reporter_)
    return;

  DCHECK_NE(adapter_status_, Status::kInitializing);
  DCHECK(als_init_status_);

  switch (*als_init_status_) {
    case AlsReader::AlsInitStatus::kSuccess:
      if (params_.metrics_key == "eve") {
        metrics_reporter_->SetDeviceClass(MetricsReporter::DeviceClass::kEve);
        return;
      }
      if (params_.metrics_key == "atlas") {
        metrics_reporter_->SetDeviceClass(MetricsReporter::DeviceClass::kAtlas);
        return;
      }
      if (params_.metrics_key == "nocturne") {
        metrics_reporter_->SetDeviceClass(
            MetricsReporter::DeviceClass::kNocturne);
        return;
      }
      if (params_.metrics_key == "kohaku") {
        metrics_reporter_->SetDeviceClass(
            MetricsReporter::DeviceClass::kKohaku);
        return;
      }
      metrics_reporter_->SetDeviceClass(
          MetricsReporter::DeviceClass::kSupportedAls);
      return;
    case AlsReader::AlsInitStatus::kDisabled:
    case AlsReader::AlsInitStatus::kMissingPath:
      metrics_reporter_->SetDeviceClass(MetricsReporter::DeviceClass::kNoAls);
      return;
    case AlsReader::AlsInitStatus::kIncorrectConfig:
      metrics_reporter_->SetDeviceClass(
          MetricsReporter::DeviceClass::kUnsupportedAls);
      return;
    case AlsReader::AlsInitStatus::kInProgress:
      NOTREACHED_IN_MIGRATION()
          << "ALS should have been initialized with a valid value.";
  }
}

Adapter::AdapterDecision Adapter::CanAdjustBrightness(base::TimeTicks now) {
  DCHECK_EQ(adapter_status_, Status::kSuccess);
  DCHECK(log_als_values_);
  DCHECK(!als_init_time_.is_null());

  AdapterDecision decision;
  const std::optional<AlsAvgStdDev> log_als_avg_stddev =
      log_als_values_->AverageAmbientWithStdDev(now);
  decision.log_als_avg_stddev = log_als_avg_stddev;

  // User has previously manually changed brightness and it (at least
  // temporarily) stopped the adapter from operating.
  if (adapter_disabled_by_user_adjustment_) {
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kDisabledByUser;
    return decision;
  }

  // Do not change brightness if it's set by the policy, but do not completely
  // disable the model as the policy could change.
  auto* prefs = profile_->GetPrefs();
  if (prefs) {
    if (prefs->GetInteger(ash::prefs::kPowerAcScreenBrightnessPercent) >= 0 ||
        prefs->GetInteger(ash::prefs::kPowerBatteryScreenBrightnessPercent) >=
            0) {
      decision.no_brightness_change_cause =
          NoBrightnessChangeCause::kBrightnessSetByPolicy;
      return decision;
    }
  }

  if (!new_model_arrived_) {
    decision.no_brightness_change_cause = NoBrightnessChangeCause::kNoNewModel;
    return decision;
  }

  // Wait until we've had enough ALS data to calc avg.
  if (now - als_init_time_ < params_.auto_brightness_als_horizon) {
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kWaitingForInitialAls;
    return decision;
  }

  if (!lid_reopen_time_.is_null()) {
    if (now - lid_reopen_time_ < lid_open_delay_time_) {
      decision.no_brightness_change_cause =
          NoBrightnessChangeCause::kWaitingForReopenAls;
      return decision;
    }

    decision.brightness_change_cause =
        BrightnessChangeCause::kUpdateAfterLidReopen;

    // Reset |lid_reopen_time_| after the first brightness change following a
    // lid-open event.
    lid_reopen_time_ = base::TimeTicks();
    return decision;
  }

  // Check if we've waited long enough from previous brightness change (either
  // by user or by model).
  if (!latest_brightness_change_time_.is_null() &&
      now - latest_brightness_change_time_ <
          params_.auto_brightness_als_horizon) {
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kWaitingForAvgHorizon;
    return decision;
  }

  if (!log_als_avg_stddev) {
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kMissingAlsData;
    return decision;
  }

  if (!average_log_ambient_lux_) {
    // Either
    // 1. brightness hasn't been changed, or,
    // 2. brightness was changed by the user but there wasn't any ALS data. This
    //    case should be rare.
    // In either case, we change brightness as soon as we have brightness.
    decision.brightness_change_cause =
        BrightnessChangeCause::kInitialAlsReceived;
    return decision;
  }

  // The following thresholds should have been set last time when brightness was
  // changed.
  DCHECK(brightening_threshold_);
  DCHECK(darkening_threshold_);

  if (log_als_avg_stddev->avg > *brightening_threshold_) {
    if (log_als_avg_stddev->stddev <= params_.brightening_log_lux_threshold *
                                          params_.stabilization_threshold) {
      decision.brightness_change_cause =
          BrightnessChangeCause::kBrightneningThresholdExceeded;
      return decision;
    }
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kFluctuatingAlsIncrease;
    return decision;
  }

  if (log_als_avg_stddev->avg < *darkening_threshold_) {
    if (log_als_avg_stddev->stddev <=
        params_.darkening_log_lux_threshold * params_.stabilization_threshold) {
      decision.brightness_change_cause =
          BrightnessChangeCause::kDarkeningThresholdExceeded;
      return decision;
    }
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kFluctuatingAlsDecrease;
    return decision;
  }

  decision.no_brightness_change_cause =
      NoBrightnessChangeCause::kMinimalAlsChange;
  return decision;
}

void Adapter::AdjustBrightness(BrightnessChangeCause cause,
                               double log_als_avg) {
  const double brightness = GetBrightnessBasedOnAmbientLogLux(log_als_avg);
  if (current_brightness_ &&
      std::abs(brightness - *current_brightness_) < kTol) {
    return;
  }

  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(brightness);
  request.set_transition(
      power_manager::SetBacklightBrightnessRequest_Transition_SLOW);
  request.set_cause(power_manager::SetBacklightBrightnessRequest_Cause_MODEL);
  chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);

  const base::TimeTicks brightness_change_time = tick_clock_->NowTicks();
  if (!latest_model_brightness_change_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES(
        "AutoScreenBrightness.BrightnessChange.ElapsedTime",
        brightness_change_time - latest_model_brightness_change_time_);
  }
  latest_model_brightness_change_time_ = brightness_change_time;
  if (current_brightness_) {
    model_brightness_change_ = brightness - *current_brightness_;
  }

  UMA_HISTOGRAM_ENUMERATION("AutoScreenBrightness.BrightnessChange.Cause",
                            cause);

  UMA_HISTOGRAM_COUNTS_1000(
      "AutoScreenBrightness.BrightnessChange.ModelIteration",
      model_.iteration_count);

  WriteLogMessages(log_als_avg, brightness, cause);
  model_brightness_change_counter_++;

  OnBrightnessChanged(brightness_change_time, brightness, log_als_avg);
}

double Adapter::GetBrightnessBasedOnAmbientLogLux(
    double ambient_log_lux) const {
  DCHECK_EQ(adapter_status_, Status::kSuccess);
  // We use the latest curve available.
  if (model_.personal_curve) {
    return model_.personal_curve->Interpolate(ambient_log_lux);
  }
  return model_.global_curve->Interpolate(ambient_log_lux);
}

void Adapter::OnBrightnessChanged(base::TimeTicks now,
                                  double new_brightness_percent,
                                  std::optional<double> new_log_als) {
  DCHECK_NE(adapter_status_, Status::kInitializing);

  current_brightness_ = new_brightness_percent;
  latest_brightness_change_time_ = now;

  UMA_HISTOGRAM_BOOLEAN("AutoScreenBrightness.MissingAlsWhenBrightnessChanged",
                        !new_log_als);

  if (!new_log_als)
    return;

  // Update |average_log_ambient_lux_| with the new reference value. Brightness
  // will be changed by the model if next log-avg ALS value goes outside of the
  // range
  // [|darkening_threshold_|, |brightening_threshold_|].
  // Thresholds in |params_| are absolute values to be added/subtracted from
  // the reference values. Log-avg can be negative.
  average_log_ambient_lux_ = new_log_als;
  brightening_threshold_ = *new_log_als + params_.brightening_log_lux_threshold;
  darkening_threshold_ = *new_log_als - params_.darkening_log_lux_threshold;
}

void Adapter::WriteLogMessages(double new_log_als,
                               double new_brightness,
                               BrightnessChangeCause cause) const {
  DCHECK_EQ(adapter_status_, Status::kSuccess);
  const std::string old_log_als =
      average_log_ambient_lux_
          ? base::StringPrintf("%.4f", average_log_ambient_lux_.value()) + "->"
          : "";

  const std::string old_brightness =
      current_brightness_ ? FormatToPrint(current_brightness_.value()) + "->"
                          : "";

  VLOG(1) << "ABAdapter screen brightness change #"
          << model_brightness_change_counter_ << ": "
          << "brightness=" << old_brightness << FormatToPrint(new_brightness)
          << " cause=" << BrightnessChangeCauseToString(cause)
          << " log_als=" << old_log_als
          << base::StringPrintf("%.4f", new_log_als);
}

void Adapter::LogAdapterDecision(
    base::TimeTicks first_recent_user_brightness_request_time,
    const AdapterDecision& decision,
    double old_brightness_percent,
    double new_brightness_percent) const {
  const bool was_previous_change_by_model =
      !latest_model_brightness_change_time_.is_null() &&
      latest_brightness_change_time_ == latest_model_brightness_change_time_;

  const bool is_decision_brightness_change =
      decision.brightness_change_cause.has_value();
  DCHECK_NE(is_decision_brightness_change,
            decision.no_brightness_change_cause.has_value());

  const std::string histogram_prefix =
      std::string("AutoScreenBrightness.AdapterDecisionAtUserChange.") +
      (is_decision_brightness_change ? "" : "No") + "BrightnessChange.";

  // What we log depends on whether the decision is to change or not to change.
  if (is_decision_brightness_change) {
    base::UmaHistogramEnumeration(histogram_prefix + "Cause",
                                  *decision.brightness_change_cause);
  } else {
    const NoBrightnessChangeCause no_brightness_change_cause =
        *decision.no_brightness_change_cause;
    base::UmaHistogramEnumeration(histogram_prefix + "Cause",
                                  no_brightness_change_cause);

    // If previous change was triggered by the model, we log additional metrics.
    if (was_previous_change_by_model) {
      UMA_HISTOGRAM_LONG_TIMES(
          "AutoScreenBrightness.ElapsedTimeBetweenModelAndUserAdjustments",
          first_recent_user_brightness_request_time -
              latest_model_brightness_change_time_);

      // If we have a recorded model change, compare it with user change and
      // log.
      if (model_brightness_change_ &&
          (no_brightness_change_cause ==
               NoBrightnessChangeCause::kMinimalAlsChange ||
           no_brightness_change_cause ==
               NoBrightnessChangeCause::kWaitingForAvgHorizon)) {
        const double user_brightness_adj =
            new_brightness_percent - old_brightness_percent;
        // Whether they are both +ve or negative.
        const bool user_model_same_direction =
            *model_brightness_change_ * user_brightness_adj >= 0;

        const int user_brightness_adj_bucket =
            ExponentialBucketing(std::max(1.0, std::abs(user_brightness_adj)));

        const int model_brightness_adj_bucket = ExponentialBucketing(
            std::max(1.0, std::abs(*model_brightness_change_)));

        const int logged_value =
            user_brightness_adj_bucket * kNumBrightnessAdjustmentBuckets +
            model_brightness_adj_bucket;

        const std::string prefix = "AutoScreenBrightness.";
        base::UmaHistogramExactLinear(
            prefix + (user_model_same_direction ? "Same" : "Opposite") +
                ".UserModelBrightnessAdjustments",
            logged_value, 100 /* value_max */);
      }
    }
  }

  // Log ALS delta and std-dev if they exist.
  if (decision.log_als_avg_stddev) {
    const int logged_stddev =
        ScaleAndConvertToInt(decision.log_als_avg_stddev->stddev);
    if (average_log_ambient_lux_) {
      const double log_als_diff =
          decision.log_als_avg_stddev->avg - *average_log_ambient_lux_;
      if (std::abs(log_als_diff) < kTol)
        return;

      const std::string dir = log_als_diff > 0 ? "Brighten" : "Darken";

      const int logged_value = ScaleAndConvertToInt(std::abs(log_als_diff));
      base::UmaHistogramCounts1000(histogram_prefix + dir + ".AlsDelta",
                                   logged_value);
      base::UmaHistogramCounts1000(histogram_prefix + dir + ".AlsStd",
                                   logged_stddev);
      return;
    }

    base::UmaHistogramCounts1000(histogram_prefix + "Unknown.AlsStd",
                                 logged_stddev);
  }

  // Log model iteration count.
  base::UmaHistogramCounts1000(
      histogram_prefix + "ModelIteration",
      model_iteration_count_at_user_brightness_change_);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

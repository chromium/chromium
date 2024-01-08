// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ADAPTER_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ADAPTER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/power/auto_screen_brightness/als_reader.h"
#include "chrome/browser/ash/power/auto_screen_brightness/als_samples.h"
#include "chrome/browser/ash/power/auto_screen_brightness/brightness_monitor.h"
#include "chrome/browser/ash/power/auto_screen_brightness/metrics_reporter.h"
#include "chrome/browser/ash/power/auto_screen_brightness/model_config.h"
#include "chrome/browser/ash/power/auto_screen_brightness/model_config_loader.h"
#include "chrome/browser/ash/power/auto_screen_brightness/modeller_impl.h"
#include "chrome/browser/ash/power/auto_screen_brightness/monotone_cubic_spline.h"
#include "chrome/browser/ash/power/auto_screen_brightness/utils.h"
#include "chromeos/dbus/power/power_manager_client.h"

class Profile;

namespace ash {
namespace power {
namespace auto_screen_brightness {

// Adapter monitors changes in ambient light, selects an optimal screen
// brightness as predicted by the model and instructs powerd to change it.
class Adapter : public AlsReader::Observer,
                public BrightnessMonitor::Observer,
                public Modeller::Observer,
                public ModelConfigLoader::Observer,
                public chromeos::PowerManagerClient::Observer {
 public:
  // How user manual brightness change will affect Adapter.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class UserAdjustmentEffect {
    // Completely disable Adapter until browser restarts.
    kDisableAuto = 0,
    // Pause Adapter until system is suspended and then resumed.
    kPauseAuto = 1,
    // No impact on Adapter and Adapter continues to auto-adjust brightness.
    kContinueAuto = 2,
    kMaxValue = kContinueAuto
  };

  // The values in Params can be overridden by experiment flags.
  // TODO(jiameng): move them to cros config json file once experiments are
  // complete.
  struct Params {
    Params();

    // Brightness is only changed if
    // 1. the log of average ambient value has gone up (resp. down) by
    //    |brightening_log_lux_threshold| (resp. |darkening_log_lux_threshold|)
    //    from the reference value. The reference value is the average ALS when
    //    brightness was changed last time (by user or model).
    //   and
    // 2. the std-dev of ALS within the averaging period is less than
    //    |stabilization_threshold| multiplied by the brightening/darkening
    //    thresholds to show the ALS has stabilized.
    double brightening_log_lux_threshold = 0.6;
    double darkening_log_lux_threshold = 0.6;
    double stabilization_threshold = 0.15;

    // Average ambient value is calculated over the past
    // |auto_brightness_als_horizon|. This is only used for brightness update,
    // which can be different from the horizon used in model training.
    base::TimeDelta auto_brightness_als_horizon = base::Seconds(4);

    UserAdjustmentEffect user_adjustment_effect =
        UserAdjustmentEffect::kPauseAuto;

    std::string metrics_key;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Status {
    kInitializing = 0,
    kSuccess = 1,
    kDisabled = 2,
    kMaxValue = kDisabled
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class BrightnessChangeCause {
    kInitialAlsReceived = 0,
    // Deprecated.
    kImmediateBrightneningThresholdExceeded = 1,
    // Deprecated.
    kImmediateDarkeningThresholdExceeded = 2,
    kBrightneningThresholdExceeded = 3,
    kDarkeningThresholdExceeded = 4,
    kUpdateAfterLidReopen = 5,
    kMaxValue = kUpdateAfterLidReopen
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class NoBrightnessChangeCause {
    kWaitingForInitialAls = 0,
    kWaitingForAvgHorizon = 1,
    // |log_als_values_| is empty.
    kMissingAlsData = 2,
    // User manually changed brightness before and it stopped adapter from
    // changing brightness.
    kDisabledByUser = 3,
    kBrightnessSetByPolicy = 4,
    // ALS increased beyond the brightening threshold, but ALS data has been
    // fluctuating above the stabilization threshold.
    kFluctuatingAlsIncrease = 5,
    // ALS decreased beyond the darkening threshold, but ALS data has been
    // fluctuating above the stabilization threshold.
    kFluctuatingAlsDecrease = 6,
    // ALS change is within darkening and brightening thresholds.
    kMinimalAlsChange = 7,
    // Adapter should only use personal curves but none is available.
    kMissingPersonalCurve = 8,
    // Adapter should only use a personal curve that has been trained for a min
    // number of iterations.
    kWaitingForTrainedPersonalCurve = 9,
    kWaitingForReopenAls = 10,
    kNoNewModel = 11,
    kMaxValue = kNoNewModel
  };

  struct AdapterDecision {
    AdapterDecision();
    AdapterDecision(const AdapterDecision& decision);
    AdapterDecision& operator=(const AdapterDecision& decision);
    // If |no_brightness_change_cause| is not nullopt, then brightness
    // should not be changed.
    // If |brightness_change_cause| is not nullopt, then brightness should be
    // changed. In this case |log_als_avg_stddev| should not be nullopt.
    // Exactly one of |no_brightness_change_cause| and
    // |brightness_change_cause| should be non-nullopt.
    // |log_als_avg_stddev| may be set even when brightness should not be
    // changed. It is only nullopt if there is no ALS data in the data cache.
    std::optional<NoBrightnessChangeCause> no_brightness_change_cause;
    std::optional<BrightnessChangeCause> brightness_change_cause;
    std::optional<AlsAvgStdDev> log_als_avg_stddev;
  };

  Adapter(Profile* profile,
          AlsReader* als_reader,
          BrightnessMonitor* brightness_monitor,
          Modeller* modeller,
          ModelConfigLoader* model_config_loader,
          MetricsReporter* metrics_reporter);

  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;

  ~Adapter() override;

  // Must be called before the Adapter is used.
  void Init();

  // AlsReader::Observer overrides:
  void OnAmbientLightUpdated(int lux) override;
  void OnAlsReaderInitialized(AlsReader::AlsInitStatus status) override;

  // BrightnessMonitor::Observer overrides:
  void OnBrightnessMonitorInitialized(bool success) override;
  void OnUserBrightnessChanged(double old_brightness_percent,
                               double new_brightness_percent) override;
  void OnUserBrightnessChangeRequested() override;

  // Modeller::Observer overrides:
  void OnModelTrained(const MonotoneCubicSpline& brightness_curve) override;
  void OnModelInitialized(const Model& model) override;

  // ModelConfigLoader::Observer overrides:
  void OnModelConfigLoaded(std::optional<ModelConfig> model_config) override;

  // chromeos::PowerManagerClient::Observer overrides:
  void PowerManagerBecameAvailable(bool service_is_ready) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;

  Status GetStatusForTesting() const;

  // Only returns true if Adapter status is success and it's not disabled by
  // user adjustment.
  bool IsAppliedForTesting() const;
  std::optional<MonotoneCubicSpline> GetGlobalCurveForTesting() const;
  std::optional<MonotoneCubicSpline> GetPersonalCurveForTesting() const;

  // Returns the average and std-dev over |log_als_values_|.
  std::optional<AlsAvgStdDev> GetAverageAmbientWithStdDevForTesting(
      base::TimeTicks now);
  double GetBrighteningThresholdForTesting() const;
  double GetDarkeningThresholdForTesting() const;

  // Returns |average_log_ambient_lux_|.
  std::optional<double> GetCurrentAvgLogAlsForTesting() const;

  static std::unique_ptr<Adapter> CreateForTesting(
      Profile* profile,
      AlsReader* als_reader,
      BrightnessMonitor* brightness_monitor,
      Modeller* modeller,
      ModelConfigLoader* model_config_loader,
      MetricsReporter* metrics_reporter,
      const base::TickClock* tick_clock);

 private:
  Adapter(Profile* profile,
          AlsReader* als_reader,
          BrightnessMonitor* brightness_monitor,
          Modeller* modeller,
          ModelConfigLoader* model_config_loader,
          MetricsReporter* metrics_reporter,
          const base::TickClock* tick_clock);

  // Called by |OnModelConfigLoaded| and only if |model_config| has been checked
  // as valid by ModelConfigLoader. It will initialize all params used by
  // the modeller from |model_config| and also other experiment flags. If
  // any param is invalid, it will disable the adapter.
  void InitParams(const ModelConfig& model_config);

  // Called to update |adapter_status_| when there's some status change from
  // AlsReader, BrightnessMonitor, Modeller, power manager and after
  // |InitParams|.
  void UpdateStatus();

  // Called after adapter is initialized. It sets metrics reporter's device
  // class if metrics reporter is set up.
  void SetMetricsReporterDeviceClass();

  // Checks whether brightness should be changed.
  // This is generally the case when the brightness hasn't been manually
  // set, we've received enough initial ambient light readings, and
  // the ambient light has changed beyond thresholds and has stabilized, and
  // also if personal curve exists (if param says we should only use personal
  // curve).
  AdapterDecision CanAdjustBrightness(base::TimeTicks now);

  // Changes the brightness. In addition to asking powerd to
  // change brightness, it also calls |OnBrightnessChanged| and writes to logs.
  void AdjustBrightness(BrightnessChangeCause cause, double log_als_avg);

  // Calculates brightness from given |ambient_log_lux| based on either
  // |model_.global_curve| or |model_.personal_curve| (as specified by the
  // experiment params). It's only safe to call this method when
  // |CanAdjustBrightness| returns a |BrightnessChangeCause| in its decision.
  double GetBrightnessBasedOnAmbientLogLux(double ambient_log_lux) const;

  // Called when brightness is changed by the model or user. This function
  // updates |latest_brightness_change_time_|, |current_brightness_|. If
  // |new_log_als| is not nullopt, it will also update
  // |average_log_ambient_lux_| and thresholds. |new_log_als| should be
  // available when this function is called, but may be nullopt when a user
  // changes brightness before any ALS reading comes in. We log an error if this
  // happens.
  void OnBrightnessChanged(base::TimeTicks now,
                           double new_brightness_percent,
                           std::optional<double> new_log_als);

  // Called by |AdjustBrightness| when brightness should be changed.
  void WriteLogMessages(double new_log_als,
                        double new_brightness,
                        BrightnessChangeCause cause) const;

  // Logs AdapterDecision. Also logs ratio of user brightness change to model
  // brightness change if previous brightness change was triggered by the model.
  // Only called when user changes brightness manually, i.e. when
  // |OnUserBrightnessChanged| is called.
  // |old_brightness_percent| and |new_brightness_percent| should come directly
  // from BrightnessMonitor, and values are between 0 and 100.
  void LogAdapterDecision(
      base::TimeTicks first_recent_user_brightness_request_time,
      const AdapterDecision& decision,
      double old_brightness_percent,
      double new_brightness_percent) const;

  const raw_ptr<Profile> profile_;

  base::ScopedObservation<AlsReader, AlsReader::Observer>
      als_reader_observation_{this};
  base::ScopedObservation<BrightnessMonitor, BrightnessMonitor::Observer>
      brightness_monitor_observation_{this};
  base::ScopedObservation<Modeller, Modeller::Observer> modeller_observation_{
      this};

  base::ScopedObservation<ModelConfigLoader, ModelConfigLoader::Observer>
      model_config_loader_observation_{this};

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};

  // Used to report daily metrics to UMA. This may be null in unit tests.
  raw_ptr<MetricsReporter> metrics_reporter_;

  Params params_;

  // This will be replaced by a mock tick clock during tests.
  raw_ptr<const base::TickClock> tick_clock_;

  // TODO(jiameng): refactor internal states and flags.

  // This buffer will be used to store the recent ambient light values in the
  // log space.
  std::unique_ptr<AmbientLightSampleBuffer> log_als_values_;

  std::optional<AlsReader::AlsInitStatus> als_init_status_;
  // Time when AlsReader is initialized.
  base::TimeTicks als_init_time_;

  std::optional<bool> brightness_monitor_success_;

  // |enabled_by_model_configs_| will remain nullopt until |OnModelConfigLoaded|
  // is called. Its value will then be set to true if the input model config
  // exists (not nullopt), and if |InitParams| properly sets params and checks
  // the model is enabled.
  std::optional<bool> enabled_by_model_configs_;

  bool model_initialized_ = false;

  std::optional<bool> power_manager_service_available_;

  // |adapter_status_| should only be set to |kDisabled| or |kSuccess| by
  // |UpdateStatus|.
  Status adapter_status_ = Status::kInitializing;

  // This is set to true whenever a user makes a manual adjustment, and if
  // |params_.user_adjustment_effect| is not |kContinueAuto|. It will be
  // reset to false if |params_.user_adjustment_effect| is |kPauseAuto|.
  // It won't be set/reset if adapter is disabled because it won't be necessary
  // to check |adapter_disabled_by_user_adjustment_|.
  bool adapter_disabled_by_user_adjustment_ = false;

  // When user changes brightness, they may press the brightness button several
  // times to reach their ideal brightness. In this process, the adapter's
  // |OnUserBrightnessChangeRequested| will be called for each button press,
  // followed by |OnUserBrightnessChanged| when user finishes with their
  // brightness change.
  // |first_recent_user_brightness_request_time_| is set to the time when the
  // first |OnUserBrightnessChangeRequested| is called, and will be unset
  // when |OnUserBrightnessChanged| is called. We use this to ensure we only
  // log the elapsed time between previous model adjustment and the 1st user
  // brightness adjustment action.
  std::optional<base::TimeTicks> first_recent_user_brightness_request_time_;
  std::optional<AdapterDecision>
      decision_at_first_recent_user_brightness_request_;

  int model_iteration_count_at_user_brightness_change_ = 0;

  // The thresholds are calculated from the |average_log_ambient_lux_|.
  // They are only updated when brightness is changed (either by user or model).
  std::optional<double> brightening_threshold_;
  std::optional<double> darkening_threshold_;

  Model model_;

  // An indicator to tell Adapter whether a curve is available to use.
  // It is set to false when a user changes brightness manually and the adapter
  // isn't already disabled by a previous user adjustment.
  // It is set to true when modeller is first initialized or when it exports a
  // new curve.
  bool new_model_arrived_ = false;

  // |average_log_ambient_lux_| is only recorded when screen brightness is
  // changed by either model or user. New thresholds will be calculated from it.
  std::optional<double> average_log_ambient_lux_;

  // Last time brightness change occurred, either by user or model.
  base::TimeTicks latest_brightness_change_time_;

  // Last time brightness was changed by the model.
  base::TimeTicks latest_model_brightness_change_time_;

  // Brightness change triggered by the model. It's only unset if model changes
  // brightness when there's no pior recorded brightness level.
  std::optional<double> model_brightness_change_;

  // Current recorded brightness. It can be either the user requested brightness
  // or the model requested brightness.
  std::optional<double> current_brightness_;

  // Used to record number of model-triggered brightness changes.
  int model_brightness_change_counter_ = 1;

  // If lid is closed then we do not record any ambient light. If a device
  // has no lid, it is considered as open.
  std::optional<bool> is_lid_closed_;

  // Ignored ALS due to closed lid is only recorded once: the 1st time when
  // ALS changes.
  bool lid_closed_message_reported_ = false;

  // Recent lid reopen time following a lid-closed event. Unset after the first
  // brightness change after a recent lid-open event.
  base::TimeTicks lid_reopen_time_;

  // ALS data that arrives soon after lid is reopened tends to be inaccurate.
  // Hence we do not store any ALS data that arrives less than
  // |lid_open_delay_time_| from |lid_reopen_time_|.
  base::TimeDelta lid_open_delay_time_ = base::Seconds(2);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ADAPTER_H_

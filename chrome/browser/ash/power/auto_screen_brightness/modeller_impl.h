// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_IMPL_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_IMPL_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/power/auto_screen_brightness/als_reader.h"
#include "chrome/browser/ash/power/auto_screen_brightness/als_samples.h"
#include "chrome/browser/ash/power/auto_screen_brightness/brightness_monitor.h"
#include "chrome/browser/ash/power/auto_screen_brightness/gaussian_trainer.h"
#include "chrome/browser/ash/power/auto_screen_brightness/model_config.h"
#include "chrome/browser/ash/power/auto_screen_brightness/model_config_loader.h"
#include "chrome/browser/ash/power/auto_screen_brightness/modeller.h"
#include "chrome/browser/ash/power/auto_screen_brightness/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

struct Model {
  Model();
  Model(const std::optional<MonotoneCubicSpline>& global_curve,
        const std::optional<MonotoneCubicSpline>& personal_curve,
        int iteration_count);
  Model(const Model& model);
  ~Model();
  std::optional<MonotoneCubicSpline> global_curve;
  std::optional<MonotoneCubicSpline> personal_curve;
  int iteration_count = 0;
};

// Real implementation of Modeller.
// It monitors user-requested brightness changes, ambient light values and
// trains personal brightness curves when user remains idle for a period of
// time.
// An object of this class must be used on the same thread that created this
// object.
class ModellerImpl : public Modeller,
                     public AlsReader::Observer,
                     public BrightnessMonitor::Observer,
                     public ModelConfigLoader::Observer,
                     public ui::UserActivityObserver {
 public:
  static constexpr char kModelDir[] = "autobrightness";
  static constexpr char kGlobalCurveFileName[] = "global_curve";
  static constexpr char kPersonalCurveFileName[] = "personal_curve";
  static constexpr char kModelIterationCountFileName[] = "iteration_count";

  // Global curve, personal curve and training iteration count will be saved to
  // the file paths below.
  struct ModelSavingSpec {
    base::FilePath global_curve;
    base::FilePath personal_curve;
    base::FilePath iteration_count;
  };

  // ModellerImpl has weak dependencies on all parameters except |trainer|.
  ModellerImpl(const Profile* profile,
               AlsReader* als_reader,
               BrightnessMonitor* brightness_monitor,
               ModelConfigLoader* model_config_loader,
               ui::UserActivityDetector* user_activity_detector,
               std::unique_ptr<Trainer> trainer);

  ModellerImpl(const ModellerImpl&) = delete;
  ModellerImpl& operator=(const ModellerImpl&) = delete;

  ~ModellerImpl() override;

  // Modeller overrides:
  void AddObserver(Modeller::Observer* observer) override;
  void RemoveObserver(Modeller::Observer* observer) override;

  // AlsReader::Observer overrides:
  void OnAmbientLightUpdated(int lux) override;
  void OnAlsReaderInitialized(AlsReader::AlsInitStatus status) override;

  // BrightnessMonitor::Observer overrides:
  void OnBrightnessMonitorInitialized(bool success) override;
  void OnUserBrightnessChanged(double old_brightness_percent,
                               double new_brightness_percent) override;
  void OnUserBrightnessChangeRequested() override;

  // ModelConfigLoader::Observer overrides:
  void OnModelConfigLoaded(std::optional<ModelConfig> model_config) override;

  // ui::UserActivityObserver overrides:
  void OnUserActivity(const ui::Event* event) override;

  // ModellerImpl has weak dependencies on all parameters except |trainer|.
  static std::unique_ptr<ModellerImpl> CreateForTesting(
      const Profile* profile,
      AlsReader* als_reader,
      BrightnessMonitor* brightness_monitor,
      ModelConfigLoader* model_config_loader,
      ui::UserActivityDetector* user_activity_detector,
      std::unique_ptr<Trainer> trainer,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const base::TickClock* tick_clock);

  // Current average log ambient light.
  std::optional<double> AverageAmbientForTesting(base::TimeTicks now);

  // Current number of training data points stored, which will be used for next
  // training.
  size_t NumberTrainingDataPointsForTesting() const;

  // Returns |max_training_data_points_| for unit tests.
  size_t GetMaxTrainingDataPointsForTesting() const;

  base::TimeDelta GetTrainingDelayForTesting() const;

  ModelConfig GetModelConfigForTesting() const;

  // Returns ModelSavingSpec used to store models. It also creates intermediate
  // directories if they do not exist. The returned paths will be empty on
  // failures.
  static ModelSavingSpec GetModelSavingSpecFromProfilePath(
      const base::FilePath& profile_path);

 private:
  // ModellerImpl has weak dependencies on all parameters except |trainer|.
  ModellerImpl(const Profile* profile,
               AlsReader* als_reader,
               BrightnessMonitor* brightness_monitor,
               ModelConfigLoader* model_config_loader,
               ui::UserActivityDetector* user_activity_detector,
               std::unique_ptr<Trainer> trainer,
               scoped_refptr<base::SequencedTaskRunner> task_runner,
               const base::TickClock* tick_clock,
               bool is_testing = false);

  // Called after we've attempted to read model saving spec from the user
  // profile.
  void OnModelSavingSpecReadFromProfile(const ModelSavingSpec& spec);

  // Called to handle a status change in one of the dependencies (ALS,
  // brightness monitor, model config loader) of the modeller. If all
  // dependencies are successfully initialized, attempts initialization of
  // the modeller (curve loading, parameter customization) and notifies
  // observers about the result.
  void HandleStatusUpdate();

  // Applies customizations from model configs. Returns whether it is
  // successful.
  bool ApplyCustomization();

  // Called as soon as |is_modeller_enabled_| has its value set. It will notify
  // all observers.
  void OnInitializationComplete();

  // Notifies a given observer about the state of the modeller. Will provide
  // either
  // - no curves (if modeller is disabled),
  // - just a global curve (if no personal curve is available), or
  // - both a global and personal curve.
  void NotifyObserverInitStatus(Modeller::Observer& observer);

  // Sets the global and personal curves based on the model read from disk. If
  // the model is invalid or not based on the current model config, instead
  // resets the global and personal curves.
  void OnModelLoadedFromDisk(const Model& model);

  void OnModelSavedToDisk(bool is_successful);

  // Called after we've set trainer's initial curves.
  void OnSetInitialCurves(bool is_personal_curve_valid);

  // Either starts training immediately or delays it for |training_delay_|.
  // Training starts immediately if |training_delay_| is 0 or number of training
  // points reached |max_training_data_points_|.
  // This function is called after a user brightness change signal is received
  // (that will be used as an example), and when a user activity is detected.
  // It's also called after initial curves are set.
  // Nothing will happen if model is not enabled.
  void ScheduleTrainerStart();

  // Starts model training and runs it in non UI thread. Also clears
  // |data_cache_|.
  void StartTraining();

  // Called after training is complete.
  void OnTrainingFinished(const TrainingResult& result);

  // Erase all info related to the personal curve.
  void ErasePersonalCurve();

  // If |is_testing_| is false, we check curve saving/loading and training jobs
  // are running on non-UI thread.
  const bool is_testing_ = false;

  // If number of recorded training data has reached |max_training_data_points_|
  // we start training immediately, without waiting for user to become idle for
  // |training_delay_|. This can be overridden by experiment flag
  // "max_training_data_points".
  size_t max_training_data_points_ = 100;

  // Once user remains idle for |training_delay_|, we start training the model.
  // If this value is 0, we will not need to wait for user to remain inactive.
  // This can be overridden by experiment flag "training_delay_in_seconds".
  base::TimeDelta training_delay_ = base::Seconds(0);

  // If personal curve error is above this threshold, the curve will not be
  // exported. The error is expressed in terms of percentages.
  double curve_error_tolerance_ = 5.0;

  base::ScopedObservation<AlsReader, AlsReader::Observer>
      als_reader_observation_{this};

  base::ScopedObservation<BrightnessMonitor, BrightnessMonitor::Observer>
      brightness_monitor_observation_{this};

  base::ScopedObservation<ModelConfigLoader, ModelConfigLoader::Observer>
      model_config_loader_observation_{this};

  base::ScopedObservation<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observation_{this};

  // Background task runner for IO work (loading a curve from disk and writing a
  // curve to disk) and training jobs.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<Trainer, base::OnTaskRunnerDeleter> trainer_;

  // This will be replaced by a mock tick clock during tests.
  const raw_ptr<const base::TickClock> tick_clock_;

  base::OneShotTimer model_timer_;

  std::optional<AlsReader::AlsInitStatus> als_init_status_;
  std::optional<bool> brightness_monitor_success_;

  // |model_config_exists_| will remain nullopt until |OnModelConfigLoaded| is
  // called. Its value will then be set to true if the input model config exists
  // (not nullopt), else its value will be false.
  std::optional<bool> model_config_exists_;
  ModelConfig model_config_;

  // Whether this modeller has initialized successfully, including connecting
  // to AlsReader, BrightnessMonitor and loading a Trainer.
  // Initially has no value. Guaranteed to have a value after the completion of
  // |OnModelLoadedFromDisk|.
  std::optional<bool> is_modeller_enabled_;

  std::optional<ModelSavingSpec> model_saving_spec_;

  // Whether the initial global curve is reset to the one constructed from
  // model config. It is true if there is no saved model loaded from the disk
  // or if the saved global curve is different from the curve from model config.
  // If this flag is true, then the global curve is saved to the disk the first
  // time a personal curve is trained and saved to disk; it will be set to false
  // after the first saving is done.
  bool global_curve_reset_ = false;

  // |model_| will be set after initialization is complete and updated each time
  // training is done with a new curve.
  Model model_;

  // |initial_global_curve_| is constructed from model config.
  std::optional<MonotoneCubicSpline> initial_global_curve_;

  // Recent log ambient values.
  std::unique_ptr<AmbientLightSampleBuffer> log_als_values_;

  std::vector<TrainingDataPoint> data_cache_;

  base::ObserverList<Modeller::Observer> observers_;

  // Training start time.
  std::optional<base::TimeTicks> training_start_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ModellerImpl> weak_ptr_factory_{this};
};

// Saves |model| to disk at location specified by |model_saving_spec| and
// returns whether it was successful. This should run in another thread to be
// non-blocking to the main thread (if |is_testing| is false).
// Not every components of |model| will be saved:
// 1. |global_curve| is saved only if |save_global_curve| is true.
// 2. |personal_curve| is saved only if |save_personal_curve| is true.
// 3. |iteration_count| is always saved.
bool SaveModelToDisk(const ModellerImpl::ModelSavingSpec& model_saving_spec,
                     const Model& model,
                     bool save_global_curve,
                     bool save_personal_curve,
                     bool is_testing);

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_IMPL_H_

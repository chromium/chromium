// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_IMPL_H_

#include <memory>

#include "base/containers/ring_buffer.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/als_reader_impl.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/brightness_monitor.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/modeller.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/trainer.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// Real implementation of Modeller.
// It monitors user-requested brightness changes, ambient light values and
// trains personal brightness curves when user remains idle for a period of
// time.
class ModellerImpl : public Modeller,
                     public AlsReader::Observer,
                     public BrightnessMonitor::Observer,
                     public ui::UserActivityObserver {
 public:
  // Once user remains idle for |kTrainingDelay|, we start training the model.
  static constexpr base::TimeDelta kTrainingDelay =
      base::TimeDelta::FromSeconds(60);

  // If number of recorded training data has reached |kMaxTrainingDataPoints| we
  // start training immediately, without waiting for user to become idle for
  // |kTrainingDelay|.
  static constexpr size_t kMaxTrainingDataPoints = 100;

  // Only train when there are at least |kMinTrainingDataPoints| training data
  // points.
  static constexpr size_t kMinTrainingDataPoints = 10;

  // TODO(jiameng): we currently use past 10 seconds of ambient values to
  // calculate average. May revise.
  static constexpr int kAmbientLightHorizonSeconds = 10;
  static constexpr base::TimeDelta kAmbientLightHorizon =
      base::TimeDelta::FromSeconds(kAmbientLightHorizonSeconds);

  // Size of |data_cache_|.
  static constexpr int kNumberAmbientValuesToTrack =
      kAmbientLightHorizonSeconds * AlsReaderImpl::kNumberAlsPollPerSeconds;

  static constexpr char kModelDir[] = "autobrightness";
  static constexpr char kCurveFileName[] = "curve";

  // ModellerImpl has weak dependencies on all parameters except |trainer|.
  ModellerImpl(Profile* profile,
               AlsReader* als_reader,
               BrightnessMonitor* brightness_monitor,
               ui::UserActivityDetector* user_activity_detector,
               std::unique_ptr<Trainer> trainer);
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

  // ui::UserActivityObserver overrides:
  void OnUserActivity(const ui::Event* event) override;

  // ModellerImpl has weak dependencies on all parameters except |trainer|.
  static std::unique_ptr<ModellerImpl> CreateForTesting(
      Profile* profile,
      AlsReader* als_reader,
      BrightnessMonitor* brightness_monitor,
      ui::UserActivityDetector* user_activity_detector,
      std::unique_ptr<Trainer> trainer,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::TickClock* tick_clock);

  // Current average ambient light.
  double AverageAmbientForTesting() const;

  // Current number of training data points stored, which will be used for next
  // training.
  size_t NumberTrainingDataPointsForTesting() const;

  // Calls GetCurvePathFromProfile directly.
  base::FilePath GetCurvePathForTesting(Profile* profile) const;

  MonotoneCubicSpline GetGlobalCurveForTesting() const;

 private:
  // ModellerImpl has weak dependencies on all parameters except |trainer|.
  ModellerImpl(Profile* profile,
               AlsReader* als_reader,
               BrightnessMonitor* brightness_monitor,
               ui::UserActivityDetector* user_activity_detector,
               std::unique_ptr<Trainer> trainer,
               scoped_refptr<base::SequencedTaskRunner> task_runner,
               const base::TickClock* tick_clock,
               bool is_testing = false);

  // Returns the path that will be used to store curves. It also creates
  // intermediate directories if they do not exist. Returns an empty path on
  // failures.
  base::FilePath GetCurvePathFromProfile(Profile* profile) const;

  // Updates |model_status_| by checking |als_init_status_| and
  // |brightness_monitor_status_| and optionally loads a curve.
  // 1. |model_status_| is |kDisabled| if either |als_init_status_| is not
  // |kSuccess|, or |brightness_monitor_success_| is false. The modeller will
  // notify its observers as soon as |model_status_| is |kDisabled|.
  // 2. If |als_init_status_| is |kSuccess| and |brightness_monitor_success_| is
  // true, then this method loads a curve from the disk and sets |model_status_|
  // to |kPersonal|. If no curve is found from the disk a default curve will be
  // created and |model_status_| is set to |kGlobal|. All observers will be
  // notified about the status and the curve.
  void HandleStatusUpdate();

  // Notifies its observers on the status of the model. It will be called either
  // when HandleStatusUpdate is called and |model_status_| is no longer
  // |kInitializing|, or when an observer is added to the modeller, and
  // |model_status_| is not |kInitializing|.
  void OnInitializationComplete();

  // Called after we've attempted to construct a |curve| from data saved on
  // disk. |curve| will be assigned to |current_curve_| if |curve| is not
  // nullopt. Otherwise, |current_curve_| will have the same value as
  // |global_curve_|.
  void OnCurveLoadedFromDisk(const base::Optional<MonotoneCubicSpline>& curve);

  // Starts |model_timer_| to start training after certain inactivity period.
  void ScheduleTrainerStart();

  // Starts model training and runs it in non UI thread. Also clears
  // |data_cache_|.
  void StartTraining();

  // Called after training is complete with a new curve.
  void OnTrainingFinished(const MonotoneCubicSpline& curve);

  // If |is_testing_| is false, we check curve saving/loading and training jobs
  // are running on non-UI thread.
  const bool is_testing_ = false;

  ScopedObserver<AlsReader, AlsReader::Observer> als_reader_observer_;

  ScopedObserver<BrightnessMonitor, BrightnessMonitor::Observer>
      brightness_monitor_observer_;

  ScopedObserver<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observer_;

  scoped_refptr<base::SequencedTaskRunner> model_task_runner_;
  std::unique_ptr<Trainer, base::OnTaskRunnerDeleter> trainer_;

  // This will be replaced by a mock tick clock during tests.
  const base::TickClock* const tick_clock_;

  base::OneShotTimer model_timer_;

  base::Optional<AlsReader::AlsInitStatus> als_init_status_;
  base::Optional<bool> brightness_monitor_success_;
  Status model_status_ = Status::kInitializing;

  base::FilePath curve_path_;

  // Latest trained curve.
  base::Optional<MonotoneCubicSpline> current_curve_;

  // Global curve constructed from predefined params.
  const MonotoneCubicSpline global_curve_;

  // Recent |kNumberAmbientValuesToTrack| ambient values.
  base::RingBuffer<AmbientLightSample, kNumberAmbientValuesToTrack>
      ambient_light_values_;

  std::vector<TrainingDataPoint> data_cache_;

  base::ObserverList<Modeller::Observer> observers_;

  base::WeakPtrFactory<ModellerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModellerImpl);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_IMPL_H_

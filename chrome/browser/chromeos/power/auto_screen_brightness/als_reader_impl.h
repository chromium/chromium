// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_IMPL_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/als_reader.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// Real implementation of AlsReader.
// It periodically reads lux values from the ambient light sensor (ALS)
// if powerd has been configured to use it.
// An object of this class must be used on the same sequence that created this
// object.
class AlsReaderImpl : public AlsReader {
 public:
  // ALS file location may not be ready immediately, so we retry every
  // |kAlsFileCheckingInterval| until |kMaxInitialAttempts| is reached, then
  // we give up.
  static constexpr base::TimeDelta kAlsFileCheckingInterval =
      base::TimeDelta::FromSeconds(1);
  static constexpr int kMaxInitialAttempts = 20;

  static constexpr base::TimeDelta kAlsPollInterval =
      base::TimeDelta::FromSecondsD(1.0 / kAlsPollFrequency);

  AlsReaderImpl();
  ~AlsReaderImpl() override;

  // AlsReader overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Checks if an ALS is enabled, and if the config is valid . Also
  // reads ambient light file path.
  void Init();

  // Sets the |blocking_task_runner_| for testing purpose.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> test_blocking_task_runner);

  // Sets ambient light path for testing purpose and initialize. This will cause
  // all the checks to be skipped, i.e. whether ALS is enabled and if config is
  // valid.
  void InitForTesting(const base::FilePath& ambient_light_path);

  // Performs all config and path-read checks in a way that will fail.
  void FailForTesting();

 private:
  friend class AlsReaderImplTest;

  // Called when we've checked whether ALS is enabled.
  void OnAlsEnableCheckDone(bool is_enabled);

  // Called when we've tried to read ALS path. If |path| is empty, it would
  // reschedule another attempt up to |kMaxInitialAttempts|.
  void OnAlsPathReadAttempted(const std::string& path);

  // Tries to read ALS path.
  void RetryAlsPath();

  // Notifies all observers with |status_| after AlsReaderImpl is initialized.
  void OnInitializationComplete();

  // Polls ambient light periodically and notifies all observers if a sample is
  // read.
  void ReadAlsPeriodically();

  // This is called after ambient light (represented as |data|) is sampled. It
  // parses |data| to int, notifies its observers and starts |als_timer_| for
  // next sample.
  void OnAlsRead(const std::string& data);

  AlsInitStatus status_ = AlsInitStatus::kInProgress;
  base::FilePath ambient_light_path_;
  int num_failed_initialization_ = 0;

  // Timer used to retry initialization and also for periodic ambient light
  // sampling.
  base::OneShotTimer als_timer_;

  // Background task runner for checking ALS status and reading ALS values.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  base::ObserverList<Observer> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AlsReaderImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AlsReaderImpl);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_IMPL_H_

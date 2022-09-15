// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_FILE_READER_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_FILE_READER_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/power/auto_screen_brightness/als_reader.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

// It periodically reads lux values from the ambient light sensor (ALS)
// if powerd has been configured to use it.
// An object of this class must be used on the same sequence that created this
// object.
class AlsFileReader : public LightProviderInterface {
 public:
  // ALS file location may not be ready immediately, so we retry every
  // |kAlsFileCheckingInterval| until |kMaxInitialAttempts| is reached, then
  // we give up.
  static constexpr base::TimeDelta kAlsFileCheckingInterval = base::Seconds(1);
  static constexpr int kMaxInitialAttempts = 20;

  static constexpr base::TimeDelta kAlsPollInterval =
      base::Seconds(1.0 / AlsReader::kAlsPollFrequency);

  explicit AlsFileReader(AlsReader* als_reader);

  AlsFileReader(const AlsFileReader&) = delete;
  AlsFileReader& operator=(const AlsFileReader&) = delete;

  ~AlsFileReader() override;

  // Checks if an ALS is enabled, and if the config is valid . Also
  // reads ambient light file path.
  void Init(scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

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
  friend class AlsFileReaderTest;

  // Called when we've tried to read ALS path. If |path| is empty, it would
  // reschedule another attempt up to |kMaxInitialAttempts|.
  void OnAlsPathReadAttempted(const std::string& path);

  // Tries to read ALS path.
  void RetryAlsPath();

  // Polls ambient light periodically and notifies all observers if a sample is
  // read.
  void ReadAlsPeriodically();

  // This is called after ambient light (represented as |data|) is sampled. It
  // parses |data| to int, notifies its observers and starts |als_timer_| for
  // next sample.
  void OnAlsRead(const std::string& data);

  base::FilePath ambient_light_path_;
  int num_failed_initialization_ = 0;

  // Timer used to retry initialization and also for periodic ambient light
  // sampling.
  base::OneShotTimer als_timer_;

  // Background task runner for checking ALS status and reading ALS values.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AlsFileReader> weak_ptr_factory_{this};
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_FILE_READER_H_

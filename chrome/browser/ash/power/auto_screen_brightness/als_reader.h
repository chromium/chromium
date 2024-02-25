// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

class AlsFileReader;
class AlsReaderTest;
class FakeLightProvider;
class LightProviderInterface;
class LightProviderMojo;
class LightSamplesObserver;

// If IIO Service is present, it uses LightProviderMojo as the implementation of
// LightProviderInterface; otherwise, it uses AlsFileReader.
class AlsReader {
 public:
  // Status of AlsReader initialization.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AlsInitStatus {
    kSuccess = 0,
    kInProgress = 1,
    kDisabled = 2,
    kIncorrectConfig = 3,
    kMissingPath = 4,
    kMaxValue = kMissingPath
  };

  // Frequency in hertz at which we read ALS samples.
  // TODO(jiameng): currently set frequency to 1hz. May revise.
  static constexpr double kAlsPollFrequency = 1.0;

  // AlsReader must outlive the observers.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;
    virtual void OnAmbientLightUpdated(int lux) = 0;
    virtual void OnAlsReaderInitialized(AlsInitStatus status) = 0;
  };

  AlsReader();
  ~AlsReader();

  void Init();

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend AlsFileReader;
  friend AlsReaderTest;
  friend FakeLightProvider;
  friend LightProviderMojo;
  friend LightSamplesObserver;

  void SetLux(int lux);
  void SetAlsInitStatus(AlsInitStatus status);
  void SetAlsInitStatusForTesting(AlsInitStatus status);

  // Background task runner for checking ALS status and reading ALS values.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::ObserverList<Observer> observers_;
  AlsInitStatus status_ = AlsInitStatus::kInProgress;
  std::unique_ptr<LightProviderInterface> provider_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AlsReader> weak_ptr_factory_{this};
};

class LightProviderInterface {
 public:
  LightProviderInterface(const LightProviderInterface&) = delete;
  LightProviderInterface& operator=(const LightProviderInterface&) = delete;
  virtual ~LightProviderInterface();

 protected:
  explicit LightProviderInterface(AlsReader* als_reader);

  raw_ptr<AlsReader, DanglingUntriaged> als_reader_;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_

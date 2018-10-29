// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_

#include "base/observer_list_types.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// Interface to ambient light reader.
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

  // AlsReader must outlive the observers.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;
    virtual void OnAmbientLightUpdated(int lux) = 0;
    virtual void OnAlsReaderInitialized(AlsInitStatus status) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  virtual ~AlsReader() = default;

  // Adds or removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_

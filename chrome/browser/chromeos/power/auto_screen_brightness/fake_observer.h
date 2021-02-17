// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_OBSERVER_H_

#include "chrome/browser/chromeos/power/auto_screen_brightness/als_reader.h"

#include "base/optional.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

class FakeObserver : public AlsReader::Observer {
 public:
  FakeObserver();
  FakeObserver(const FakeObserver&) = delete;
  FakeObserver& operator=(const FakeObserver&) = delete;
  ~FakeObserver() override;

  // AlsReader::Observer overrides:
  void OnAmbientLightUpdated(int lux) override;

  void OnAlsReaderInitialized(AlsReader::AlsInitStatus status) override;

  inline int ambient_light() const { return ambient_light_; }
  inline int num_received_ambient_lights() const {
    return num_received_ambient_lights_;
  }
  inline bool has_status() const { return status_.has_value(); }
  inline AlsReader::AlsInitStatus status() const {
    DCHECK(status_);
    return status_.value();
  }

 private:
  int ambient_light_ = -1;
  int num_received_ambient_lights_ = 0;
  base::Optional<AlsReader::AlsInitStatus> status_;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_OBSERVER_H_

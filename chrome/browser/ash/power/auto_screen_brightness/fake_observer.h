// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_OBSERVER_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_OBSERVER_H_

#include <optional>

#include "chrome/browser/ash/power/auto_screen_brightness/als_reader.h"

namespace ash {
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
  std::optional<AlsReader::AlsInitStatus> status_;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_OBSERVER_H_

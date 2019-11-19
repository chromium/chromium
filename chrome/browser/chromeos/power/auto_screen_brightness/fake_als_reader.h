// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_ALS_READER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_ALS_READER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/als_reader.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// This is a fake AlsReader used for testing only.
class FakeAlsReader : public AlsReader {
 public:
  FakeAlsReader();
  ~FakeAlsReader() override;

  void set_als_init_status(AlsInitStatus status) { status_ = status; }

  void ReportReaderInitialized();
  void ReportAmbientLightUpdate(int lux);

  // AlsReader overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  AlsInitStatus status_ = AlsInitStatus::kInProgress;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<FakeAlsReader> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeAlsReader);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_ALS_READER_H_

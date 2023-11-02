// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_LIGHT_PROVIDER_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_LIGHT_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/power/auto_screen_brightness/als_reader.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

// This is a fake implementation of LightProviderInterface for testing only.
class FakeLightProvider : public LightProviderInterface {
 public:
  explicit FakeLightProvider(AlsReader* als_reader);
  FakeLightProvider(const FakeLightProvider&) = delete;
  FakeLightProvider& operator=(const FakeLightProvider&) = delete;
  ~FakeLightProvider() override;

  void set_als_init_status(AlsReader::AlsInitStatus status) {
    DCHECK(als_reader_);
    status_ = status;
    als_reader_->SetAlsInitStatusForTesting(status_);
  }

  void ReportReaderInitialized();
  void ReportAmbientLightUpdate(int lux);

 private:
  AlsReader::AlsInitStatus status_ = AlsReader::AlsInitStatus::kInProgress;
  base::WeakPtrFactory<FakeLightProvider> weak_ptr_factory_{this};
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_LIGHT_PROVIDER_H_

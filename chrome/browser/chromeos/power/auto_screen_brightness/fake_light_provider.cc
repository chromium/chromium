// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_light_provider.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

FakeLightProvider::FakeLightProvider(AlsReader* als_reader)
    : LightProviderInterface(als_reader) {}

FakeLightProvider::~FakeLightProvider() = default;

void FakeLightProvider::ReportAmbientLightUpdate(int lux) {
  DCHECK(als_reader_);
  als_reader_->SetLux(lux);
}

void FakeLightProvider::ReportReaderInitialized() {
  DCHECK(als_reader_);
  als_reader_->SetAlsInitStatus(status_);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

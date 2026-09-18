// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/fake_light_provider.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

FakeLightProvider::FakeLightProvider(AlsReader* als_reader)
    : LightProviderInterface(als_reader) {}

FakeLightProvider::~FakeLightProvider() = default;

void FakeLightProvider::ReportAmbientLightUpdate(int lux) {
  CHECK(als_reader_, base::NotFatalUntil::M160);
  als_reader_->SetLux(lux);
}

void FakeLightProvider::ReportReaderInitialized() {
  CHECK(als_reader_, base::NotFatalUntil::M160);
  als_reader_->SetAlsInitStatus(status_);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

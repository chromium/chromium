// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/fake_observer.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

FakeObserver::FakeObserver() = default;
FakeObserver::~FakeObserver() = default;

void FakeObserver::OnAmbientLightUpdated(int lux) {
  ambient_light_ = lux;
  ++num_received_ambient_lights_;
}

void FakeObserver::OnAlsReaderInitialized(AlsReader::AlsInitStatus status) {
  status_ = status;
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

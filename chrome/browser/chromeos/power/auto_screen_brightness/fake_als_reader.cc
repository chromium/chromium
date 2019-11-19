// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_als_reader.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

FakeAlsReader::FakeAlsReader() {}

FakeAlsReader::~FakeAlsReader() = default;

void FakeAlsReader::ReportAmbientLightUpdate(const int lux) {
  for (auto& observer : observers_)
    observer.OnAmbientLightUpdated(lux);
}

void FakeAlsReader::ReportReaderInitialized() {
  for (auto& observer : observers_)
    observer.OnAlsReaderInitialized(status_);
}

void FakeAlsReader::AddObserver(Observer* const observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (status_ != AlsInitStatus::kInProgress) {
    observer->OnAlsReaderInitialized(status_);
  }
}

void FakeAlsReader::RemoveObserver(Observer* const observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_model_config_loader.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

FakeModelConfigLoader::FakeModelConfigLoader() {}

FakeModelConfigLoader::~FakeModelConfigLoader() = default;

void FakeModelConfigLoader::ReportModelConfigLoaded() {
  DCHECK(is_initialized_);
  for (auto& observer : observers_) {
    NotifyObserver(&observer);
  }
}

void FakeModelConfigLoader::AddObserver(Observer* const observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (is_initialized_) {
    NotifyObserver(observer);
  }
}

void FakeModelConfigLoader::RemoveObserver(Observer* const observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void FakeModelConfigLoader::NotifyObserver(Observer* const observer) {
  DCHECK(observer);
  observer->OnModelConfigLoaded(is_model_config_valid_
                                    ? base::Optional<ModelConfig>(model_config_)
                                    : base::nullopt);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

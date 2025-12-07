// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/scoped_fake_power_status.h"

#include <memory>

#include "ash/system/model/fake_power_status.h"
#include "ash/system/power/power_status.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"

namespace ash {

// static
ScopedFakePowerStatus* ScopedFakePowerStatus::instance_ = nullptr;

ScopedFakePowerStatus::ScopedFakePowerStatus() {
  // Only allow one scoped instance at a time.
  CHECK(!instance_);
  instance_ = this;

  real_power_status_instance_ = PowerStatus::g_power_status_;

  // Create a fake model and replace it with the real one.
  fake_power_status_ = std::make_unique<FakePowerStatus>();

  PowerStatus::Get()->g_power_status_ = fake_power_status_.get();
}

ScopedFakePowerStatus::~ScopedFakePowerStatus() {
  if (instance_ != this) {
    NOTREACHED();
  }

  instance_ = nullptr;
  PowerStatus::Get()->g_power_status_ = real_power_status_instance_;
  PowerStatus::Get()->RequestStatusUpdate();
}

}  // namespace ash

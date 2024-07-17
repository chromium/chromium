// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_SCOPED_FAKE_POWER_STATUS_H_
#define ASH_SYSTEM_MODEL_SCOPED_FAKE_POWER_STATUS_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class FakePowerStatus;
class PowerStatus;

// Creates a fake `PowerStatus` and use that fake instance to be the
// singleton object for `PowerStatus::Get()`. The real instance
// will be restored when this scoped object is destructed.
class ASH_EXPORT ScopedFakePowerStatus {
 public:
  ScopedFakePowerStatus();

  ScopedFakePowerStatus(const ScopedFakePowerStatus&) = delete;
  ScopedFakePowerStatus& operator=(const ScopedFakePowerStatus&) = delete;

  ~ScopedFakePowerStatus();

  FakePowerStatus* fake_power_status() { return fake_power_status_.get(); }

 private:
  static ScopedFakePowerStatus* instance_;

  std::unique_ptr<FakePowerStatus> fake_power_status_;

  raw_ptr<PowerStatus> real_power_status_instance_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_SCOPED_FAKE_POWER_STATUS_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FAKE_HATS_BLUETOOTH_REVAMP_TRIGGER_IMPL_H_
#define ASH_PUBLIC_CPP_FAKE_HATS_BLUETOOTH_REVAMP_TRIGGER_IMPL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"

namespace ash {

// Fake implementation of `HatsBluetoothRevampTrigger`, used in test.
class ASH_PUBLIC_EXPORT FakeHatsBluetoothRevampTriggerImpl
    : public HatsBluetoothRevampTrigger {
 public:
  FakeHatsBluetoothRevampTriggerImpl();
  FakeHatsBluetoothRevampTriggerImpl(
      const FakeHatsBluetoothRevampTriggerImpl&) = delete;
  const FakeHatsBluetoothRevampTriggerImpl& operator=(
      const FakeHatsBluetoothRevampTriggerImpl&) = delete;
  ~FakeHatsBluetoothRevampTriggerImpl() override = default;

  bool try_to_show_survey_called() const { return try_to_show_survey_called_; }

  // HatsBluetoothRevampTrigger:
  void TryToShowSurvey() override;

 private:
  // True if `TryToShowSurvey()` was called.
  bool try_to_show_survey_called_ = false;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FAKE_HATS_BLUETOOTH_REVAMP_TRIGGER_IMPL_H_
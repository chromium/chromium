// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HATS_BLUETOOTH_REVAMP_TRIGGER_H_
#define ASH_PUBLIC_CPP_HATS_BLUETOOTH_REVAMP_TRIGGER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Used to show a Happiness Tracking Survey after user interacts with revamped
// Bluetooth UI surfaces. Implementation of this class is in
// //chrome/browser/ash/bluetooth.
class ASH_PUBLIC_EXPORT HatsBluetoothRevampTrigger {
 public:
  // Gets the global instance.
  static HatsBluetoothRevampTrigger* Get();

  HatsBluetoothRevampTrigger(const HatsBluetoothRevampTrigger&) = delete;
  HatsBluetoothRevampTrigger& operator=(const HatsBluetoothRevampTrigger&) =
      delete;

  // Checks to see if a survey should be shown, and if so, displays the survey
  // with some probability determined by the Finch config file.
  virtual void TryToShowSurvey() = 0;

 protected:
  HatsBluetoothRevampTrigger();
  virtual ~HatsBluetoothRevampTrigger();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HATS_BLUETOOTH_REVAMP_TRIGGER_H_
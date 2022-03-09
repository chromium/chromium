// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_PAIRING_STATE_TRACKER_H_
#define ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_PAIRING_STATE_TRACKER_H_

#include "ash/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"

namespace ash {
namespace multidevice_setup {

class FakeAndroidSmsPairingStateTracker : public AndroidSmsPairingStateTracker {
 public:
  FakeAndroidSmsPairingStateTracker();

  FakeAndroidSmsPairingStateTracker(const FakeAndroidSmsPairingStateTracker&) =
      delete;
  FakeAndroidSmsPairingStateTracker& operator=(
      const FakeAndroidSmsPairingStateTracker&) = delete;

  ~FakeAndroidSmsPairingStateTracker() override;
  void SetPairingComplete(bool is_pairing_complete);

  // AndroidSmsPairingStateTracker:
  bool IsAndroidSmsPairingComplete() override;

 private:
  bool is_pairing_complete_ = false;
};

}  // namespace multidevice_setup
}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos::multidevice_setup {
using ::ash::multidevice_setup::FakeAndroidSmsPairingStateTracker;
}

#endif  // ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_PAIRING_STATE_TRACKER_H_

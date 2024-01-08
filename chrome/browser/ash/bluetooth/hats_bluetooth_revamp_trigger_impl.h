// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BLUETOOTH_HATS_BLUETOOTH_REVAMP_TRIGGER_IMPL_H_
#define CHROME_BROWSER_ASH_BLUETOOTH_HATS_BLUETOOTH_REVAMP_TRIGGER_IMPL_H_

#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"

class Profile;
class PrefRegistrySimple;

namespace ash {

// Implementation for HatsBluetoothRevampTrigger.
class HatsBluetoothRevampTriggerImpl : public HatsBluetoothRevampTrigger {
 public:
  HatsBluetoothRevampTriggerImpl();
  HatsBluetoothRevampTriggerImpl(const HatsBluetoothRevampTriggerImpl&) =
      delete;
  HatsBluetoothRevampTriggerImpl& operator=(
      const HatsBluetoothRevampTriggerImpl&) = delete;
  ~HatsBluetoothRevampTriggerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // HatsBluetoothRevampTrigger:
  void TryToShowSurvey() override;

  void set_profile_for_testing(Profile* profile) {
    profile_for_testing_ = profile;
    did_set_profile_for_testing_ = true;
  }

  base::OneShotTimer* timer_for_testing() { return &hats_timer_; }

 private:
  void ShowSurvey();
  Profile* GetActiveUserProfile();

  base::OneShotTimer hats_timer_;
  raw_ptr<Profile, DanglingUntriaged> profile_for_testing_ = nullptr;
  bool did_set_profile_for_testing_ = false;
  scoped_refptr<ash::HatsNotificationController> hats_notification_controller_;
  base::WeakPtrFactory<HatsBluetoothRevampTriggerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BLUETOOTH_HATS_BLUETOOTH_REVAMP_TRIGGER_IMPL_H_

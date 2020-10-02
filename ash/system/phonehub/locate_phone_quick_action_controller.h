// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_LOCATE_PHONE_QUICK_ACTION_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_LOCATE_PHONE_QUICK_ACTION_CONTROLLER_H_

#include "ash/system/phonehub/quick_action_controller_base.h"
#include "chromeos/components/phonehub/find_my_device_controller.h"

namespace ash {

// Controller of a quick action item that toggles Locate phone mode.
class LocatePhoneQuickActionController
    : public QuickActionControllerBase,
      public chromeos::phonehub::FindMyDeviceController::Observer {
 public:
  explicit LocatePhoneQuickActionController(
      chromeos::phonehub::FindMyDeviceController* find_my_device_controller);
  ~LocatePhoneQuickActionController() override;
  LocatePhoneQuickActionController(LocatePhoneQuickActionController&) = delete;
  LocatePhoneQuickActionController operator=(
      LocatePhoneQuickActionController&) = delete;

  // QuickActionControllerBase:
  QuickActionItem* CreateItem() override;
  void OnButtonPressed(bool is_now_enabled) override;

  // chromeos::phonehub::FindMyDeviceController::Observer:
  void OnPhoneRingingStateChanged() override;

 private:
  // All the possible states that the locate phone button can be viewed. Each
  // state has a corresponding icon, labels and tooltip view.
  enum class ActionState { kOff, kConnecting, kOn };

  // Set the item (including icon, label and tooltips) to a certain state.
  void SetState(ActionState state);

  chromeos::phonehub::FindMyDeviceController* find_my_device_controller_ =
      nullptr;
  QuickActionItem* item_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_LOCATE_PHONE_QUICK_ACTION_CONTROLLER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_LOCATE_PHONE_QUICK_ACTION_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_LOCATE_PHONE_QUICK_ACTION_CONTROLLER_H_

#include "ash/system/phonehub/quick_action_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/find_my_device_controller.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace ash {

// Controller of a quick action item that toggles Locate phone mode.
class LocatePhoneQuickActionController
    : public QuickActionControllerBase,
      public phonehub::FindMyDeviceController::Observer {
 public:
  LocatePhoneQuickActionController(
      phonehub::FindMyDeviceController* find_my_device_controller);
  ~LocatePhoneQuickActionController() override;
  LocatePhoneQuickActionController(LocatePhoneQuickActionController&) = delete;
  LocatePhoneQuickActionController operator=(
      LocatePhoneQuickActionController&) = delete;

  // QuickActionControllerBase:
  QuickActionItem* CreateItem() override;
  void OnButtonPressed(bool is_now_enabled) override;
  void UpdateQuickActionItemUi() override;

  // phonehub::FindMyDeviceController::Observer:
  void OnPhoneRingingStateChanged() override;

 private:
  // All the possible states that the locate phone button can be viewed. Each
  // state has a corresponding icon, labels and tooltip view.
  enum class ActionState { kNotAvailable, kOff, kOn };

  // Compute and update the state of the item according to
  // FindMyDeviceController.
  void UpdateState();

  // Set the item (including icon, label and tooltips) to a certain state.
  void SetItemState(ActionState state);

  // Check to see if the requested state is the same as the current state of the
  // phone. Make changes to item's state if necessary.
  void CheckRequestedState();

  raw_ptr<phonehub::FindMyDeviceController> find_my_device_controller_ =
      nullptr;
  raw_ptr<QuickActionItem> item_ = nullptr;

  // Keep track the current state of the item.
  ActionState state_ = ActionState::kOff;

  // State that user requests when clicking the button.
  std::optional<ActionState> requested_state_;

  // Timer that fires to prevent showing wrong state in the item. It will check
  // if the requested state is the same as the current state after the button is
  // pressed for a certain time.
  std::unique_ptr<base::OneShotTimer> check_requested_state_timer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_LOCATE_PHONE_QUICK_ACTION_CONTROLLER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_SILENCE_PHONE_QUICK_ACTION_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_SILENCE_PHONE_QUICK_ACTION_CONTROLLER_H_

#include "ash/system/phonehub/quick_action_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/do_not_disturb_controller.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace ash {

// Controller of a quick action item that toggles silence phone mode.
class ASH_EXPORT SilencePhoneQuickActionController
    : public QuickActionControllerBase,
      public phonehub::DoNotDisturbController::Observer {
 public:
  explicit SilencePhoneQuickActionController(
      phonehub::DoNotDisturbController* dnd_controller);
  ~SilencePhoneQuickActionController() override;
  SilencePhoneQuickActionController(SilencePhoneQuickActionController&) =
      delete;
  SilencePhoneQuickActionController operator=(
      SilencePhoneQuickActionController&) = delete;

  // Return true if the item is enabled/toggled.
  bool IsItemEnabled();

  // QuickActionControllerBase:
  QuickActionItem* CreateItem() override;
  void OnButtonPressed(bool is_now_enabled) override;
  void UpdateQuickActionItemUi() override;

  // phonehub::DoNotDisturbController::Observer:
  void OnDndStateChanged() override;

 private:
  friend class SilencePhoneQuickActionControllerTest;

  // All the possible states that the silence phone button can be viewed. Each
  // state has a corresponding icon, labels and tooltip view.
  enum class ActionState { kOff, kOn, kDisabled };

  // Set the item (including icon, label and tooltips) to a certain state.
  void SetItemState(ActionState state);

  // Retrieves the current state of the QuickActionItem. Used only for testing.
  ActionState GetItemState();

  // Check to see if the requested state is similar to current state of the
  // phone. Make changes to item's state if necessary.
  void CheckRequestedState();

  raw_ptr<phonehub::DoNotDisturbController> dnd_controller_ = nullptr;
  raw_ptr<QuickActionItem, DanglingUntriaged> item_ = nullptr;

  // Keep track the current state of the item.
  ActionState state_;

  // State that user requests when clicking the button.
  std::optional<ActionState> requested_state_;

  // Timer that fires to prevent showing wrong state in the item. It will check
  // if the requested state is similar to the current state after the button is
  // pressed for a certain time.
  std::unique_ptr<base::OneShotTimer> check_requested_state_timer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_SILENCE_PHONE_QUICK_ACTION_CONTROLLER_H_

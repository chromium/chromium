// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_SILENCE_PHONE_QUICK_ACTION_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_SILENCE_PHONE_QUICK_ACTION_CONTROLLER_H_

#include "ash/system/phonehub/quick_action_controller_base.h"
#include "chromeos/components/phonehub/do_not_disturb_controller.h"

namespace ash {

// Controller of a quick action item that toggles silence phone mode.
class SilencePhoneQuickActionController
    : public QuickActionControllerBase,
      public chromeos::phonehub::DoNotDisturbController::Observer {
 public:
  explicit SilencePhoneQuickActionController(
      chromeos::phonehub::DoNotDisturbController* dnd_controller);
  ~SilencePhoneQuickActionController() override;
  SilencePhoneQuickActionController(SilencePhoneQuickActionController&) =
      delete;
  SilencePhoneQuickActionController operator=(
      SilencePhoneQuickActionController&) = delete;

  // QuickActionControllerBase:
  QuickActionItem* CreateItem() override;
  void OnButtonPressed(bool is_now_enabled) override;

  // chromeos::phonehub::DoNotDisturbController::Observer:
  void OnDndStateChanged() override;

 private:
  // All the possible states that the silence phone button can be viewed. Each
  // state has a corresponding icon, labels and tooltip view.
  enum class ActionState { kOff, kConnecting, kOn };

  // Set the item (including icon, label and tooltips) to a certain state.
  void SetState(ActionState state);

  chromeos::phonehub::DoNotDisturbController* dnd_controller_ = nullptr;
  QuickActionItem* item_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_SILENCE_PHONE_QUICK_ACTION_CONTROLLER_H_

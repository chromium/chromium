// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_MULTITASK_MENU_NUDGE_DELEGATE_ASH_H_
#define ASH_FRAME_MULTITASK_MENU_NUDGE_DELEGATE_ASH_H_

#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"

namespace ash {

// Ash implementation of nudge controller delegate that lets us get and set pref
// values from the ash active profile.
class MultitaskMenuNudgeDelegateAsh
    : public chromeos::MultitaskMenuNudgeController::Delegate {
 public:
  using GetPreferencesCallback =
      chromeos::MultitaskMenuNudgeController::GetPreferencesCallback;

  static constexpr int kTabletNudgeAdditionalYOffset = 6;

  MultitaskMenuNudgeDelegateAsh();
  MultitaskMenuNudgeDelegateAsh(const MultitaskMenuNudgeDelegateAsh&) = delete;
  MultitaskMenuNudgeDelegateAsh& operator=(
      const MultitaskMenuNudgeDelegateAsh&) = delete;
  ~MultitaskMenuNudgeDelegateAsh() override;

  // chromeos::MultitaskMenuNudgeController::Delegate:
  int GetTabletNudgeYOffset() const override;
  void GetNudgePreferences(bool tablet_mode,
                           GetPreferencesCallback callback) override;
  void SetNudgePreferences(bool tablet_mode,
                           int count,
                           base::Time time) override;
};

}  // namespace ash

#endif  // ASH_FRAME_MULTITASK_MENU_NUDGE_DELEGATE_ASH_H_

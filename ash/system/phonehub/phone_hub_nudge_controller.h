// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_NUDGE_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {
// This class controls showing a nudge when a user is eligible for Phone Hub.
class ASH_EXPORT PhoneHubNudgeController {
 public:
  PhoneHubNudgeController();
  PhoneHubNudgeController(const PhoneHubNudgeController&) = delete;
  PhoneHubNudgeController& operator=(const PhoneHubNudgeController&) = delete;
  ~PhoneHubNudgeController();

  void ShowNudge(views::View* anchor_view, const std::u16string& text);
  void HideNudge();

  // Attempts recording nudge action metric when Phone Hub icon is activated.
  void MaybeRecordNudgeAction();
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_NUDGE_CONTROLLER_H_
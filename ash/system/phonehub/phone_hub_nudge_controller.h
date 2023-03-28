// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_NUDGE_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_NUDGE_CONTROLLER_H_

#include "ash/system/tray/system_nudge_controller.h"

namespace ash {
// This class controls showing a nudge when a user is eligible for Phone Hub.
class ASH_EXPORT PhoneHubNudgeController : public SystemNudgeController {
 public:
  PhoneHubNudgeController();
  PhoneHubNudgeController(const PhoneHubNudgeController&) = delete;
  PhoneHubNudgeController& operator=(const PhoneHubNudgeController&) = delete;
  ~PhoneHubNudgeController() override;

  void SetNudgeContent();

 protected:
  // SystemNudgeController: Creates PhoneHubNudge
  std::unique_ptr<SystemNudge> CreateSystemNudge() override;

 private:
  std::u16string nudge_content_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_NUDGE_CONTROLLER_H_
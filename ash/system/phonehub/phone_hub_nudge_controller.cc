// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_nudge_controller.h"

#include "ash/system/phonehub/phone_hub_nudge.h"

namespace ash {

PhoneHubNudgeController::PhoneHubNudgeController(std::u16string nudge_content)
    : nudge_content_(nudge_content) {}
PhoneHubNudgeController::~PhoneHubNudgeController() = default;

std::unique_ptr<SystemNudge> PhoneHubNudgeController::CreateSystemNudge() {
  return std::make_unique<PhoneHubNudge>(nudge_content_);
}

}  // namespace ash
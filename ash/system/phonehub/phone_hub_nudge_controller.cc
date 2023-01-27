// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_nudge_controller.h"

#include "ash/system/phonehub/phone_hub_nudge.h"

namespace ash {

PhoneHubNudgeController::PhoneHubNudgeController() = default;
PhoneHubNudgeController::~PhoneHubNudgeController() = default;

std::unique_ptr<SystemNudge> PhoneHubNudgeController::CreateSystemNudge() {
  SetNudgeContent();
  return std::make_unique<PhoneHubNudge>(nudge_content_);
}

void PhoneHubNudgeController::SetNudgeContent() {
  nudge_content_ = u"";
}
}  // namespace ash
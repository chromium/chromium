// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_nudge_controller.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"

namespace ash {
namespace {
const std::string kPhoneHubNudgeId = "PhoneHubNudge";

}  // namespace
PhoneHubNudgeController::PhoneHubNudgeController() = default;
PhoneHubNudgeController::~PhoneHubNudgeController() = default;

void PhoneHubNudgeController::ShowNudge(views::View* anchor_view,
                                        const std::u16string& text) {
  if (!ash::features::IsPhoneHubNudgeEnabled()) {
    return;
  }
  AnchoredNudgeData nudge_data = {kPhoneHubNudgeId, NudgeCatalogName::kPhoneHub,
                                  text, anchor_view};
  AnchoredNudgeManager::Get()->Show(nudge_data);
}

void PhoneHubNudgeController::HideNudge() {
  if (!ash::features::IsPhoneHubNudgeEnabled()) {
    return;
  }
  AnchoredNudgeManager::Get()->Cancel(kPhoneHubNudgeId);
}

void PhoneHubNudgeController::MaybeRecordNudgeAction() {
  AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
      NudgeCatalogName::kPhoneHub);
}

}  // namespace ash
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/onboarding_nudge_controller.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/phonehub/phone_hub_ui_controller.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {
const std::string kPhoneHubNudgeId = "PhoneHubNudge";

bool IsInUserSession() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !session_controller->IsRunningInAppMode();
}

}  // namespace
OnboardingNudgeController::OnboardingNudgeController(
    PhoneHubTray* phone_hub_tray,
    base::RepeatingClosure stop_animation_callback,
    base::RepeatingClosure start_animation_callback)
    : phone_hub_tray_(phone_hub_tray),
      stop_animation_callback_(std::move(stop_animation_callback)),
      start_animation_callback_(std::move(start_animation_callback)) {}

OnboardingNudgeController::~OnboardingNudgeController() = default;

void OnboardingNudgeController::ShowNudgeIfNeeded() {
  if (!features::IsPhoneHubNudgeEnabled() ||
      phone_hub_tray_->ui_controller()->ui_state() !=
          PhoneHubUiController::UiState::kOnboardingWithoutPhone ||
      !IsInUserSession()) {
    return;
  }
  // TODO(b/282057052): update text based on different groups.
  std::u16string nudge_text = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_NOTIFIER_TEXT_WITH_PHONE_HUB);
  AnchoredNudgeData nudge_data = {kPhoneHubNudgeId, NudgeCatalogName::kPhoneHub,
                                  nudge_text, phone_hub_tray_};
  nudge_data.anchored_to_shelf = true;
  nudge_data.nudge_dimiss_callback = stop_animation_callback_;
  AnchoredNudgeManager::Get()->Show(nudge_data);
  start_animation_callback_.Run();
}

void OnboardingNudgeController::HideNudge() {
  if (!features::IsPhoneHubNudgeEnabled()) {
    return;
  }
  AnchoredNudgeManager::Get()->Cancel(kPhoneHubNudgeId);
}

void OnboardingNudgeController::MaybeRecordNudgeAction() {
  AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
      NudgeCatalogName::kPhoneHub);
}

}  // namespace ash

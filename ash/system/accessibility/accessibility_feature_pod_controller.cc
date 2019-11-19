// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/accessibility_feature_pod_controller.h"

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

AccessibilityFeaturePodController::AccessibilityFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

AccessibilityFeaturePodController::~AccessibilityFeaturePodController() =
    default;

FeaturePodButton* AccessibilityFeaturePodController::CreateButton() {
  auto* button = new FeaturePodButton(this, /*is_togglable=*/false);
  button->SetID(ash::VIEW_ID_ACCESSIBILITY_TRAY_ITEM);
  button->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY));
  button->SetVectorIcon(kUnifiedMenuAccessibilityIcon);
  button->SetIconAndLabelTooltips(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TOOLTIP));
  button->ShowDetailedViewArrow();
  button->DisableLabelButtonFocus();

  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  LoginStatus login_status = Shell::Get()->session_controller()->login_status();
  button->SetVisible(login_status == LoginStatus::NOT_LOGGED_IN ||
                     login_status == LoginStatus::LOCKED ||
                     delegate->ShouldShowAccessibilityMenu());
  return button;
}

void AccessibilityFeaturePodController::OnIconPressed() {
  tray_controller_->ShowAccessibilityDetailedView();
}

SystemTrayItemUmaType AccessibilityFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_ACCESSIBILITY;
}

}  // namespace ash

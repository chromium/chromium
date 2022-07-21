// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/autozoom_feature_pod_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

AutozoomFeaturePodController::AutozoomFeaturePodController() {
  auto* camera_hal_dispatcher = media::CameraHalDispatcherImpl::GetInstance();
  if (camera_hal_dispatcher) {
    camera_hal_dispatcher->AddActiveClientObserver(this);
  }
}

AutozoomFeaturePodController::~AutozoomFeaturePodController() {
  auto* camera_hal_dispatcher = media::CameraHalDispatcherImpl::GetInstance();
  if (camera_hal_dispatcher) {
    camera_hal_dispatcher->RemoveActiveClientObserver(this);
  }
}

FeaturePodButton* AutozoomFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->SetVectorIcon(kUnifiedMenuAutozoomIcon);

  button_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_BUTTON_LABEL));
  auto description = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_AUTOZOOM_TOGGLE_ACCESSIBILITY_DESCRIPTION);
  button_->icon_button()->GetViewAccessibility().OverrideDescription(
      description);
  button_->label_button()->GetViewAccessibility().OverrideDescription(
      description);
  UpdateButton();
  return button_;
}

SystemTrayItemUmaType AutozoomFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_AUTOZOOM;
}

void AutozoomFeaturePodController::OnToggled() {
  Shell::Get()->autozoom_controller()->Toggle();
  UpdateButton();
}

void AutozoomFeaturePodController::OnLabelPressed() {
  if (!button_->GetEnabled())
    return;
  OnToggled();
}

void AutozoomFeaturePodController::OnIconPressed() {
  OnToggled();
}

void AutozoomFeaturePodController::UpdateButtonVisibility() {
  if (!button_)
    return;

  button_->SetVisible(
      Shell::Get()->session_controller()->ShouldEnableSettings() &&
      active_camera_client_count_ > 0);
}

void AutozoomFeaturePodController::UpdateButton() {
  auto state = Shell::Get()->autozoom_controller()->GetState();

  button_->SetToggled(state != cros::mojom::CameraAutoFramingState::OFF);
  UpdateButtonVisibility();

  std::u16string tooltip_state;
  std::u16string button_label;

  switch (state) {
    case cros::mojom::CameraAutoFramingState::OFF:
      button_label =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_OFF_STATE);
      tooltip_state = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUTOZOOM_OFF_STATE_TOOLTIP);
      break;
    case cros::mojom::CameraAutoFramingState::ON_SINGLE:
    case cros::mojom::CameraAutoFramingState::ON_MULTI:
      button_label =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_ON_STATE);
      tooltip_state = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUTOZOOM_ON_STATE_TOOLTIP);
      break;
  }

  button_->SetSubLabel(button_label);
  button_->SetIconAndLabelTooltips(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_AUTOZOOM_TOGGLE_TOOLTIP, tooltip_state));
}

void AutozoomFeaturePodController::OnActiveClientChange(
    cros::mojom::CameraClientType type,
    bool is_active) {
  if (is_active) {
    active_camera_client_count_++;
  } else {
    active_camera_client_count_--;
  }

  UpdateButtonVisibility();
}

}  // namespace ash

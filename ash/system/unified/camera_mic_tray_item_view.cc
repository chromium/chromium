// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/camera_mic_tray_item_view.h"

#include <algorithm>
#include <string>

#include "ash/public/cpp/media_controller.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/image_view.h"

namespace ash {

CameraMicTrayItemView::CameraMicTrayItemView(Shelf* shelf, Type type)
    : TrayItemView(shelf), type_(type) {
  CreateImageView();
  FetchMessage();

  if (!chromeos::features::IsJellyEnabled()) {
    image_view()->SetImage(gfx::CreateVectorIcon(gfx::IconDescription(
        GetIcon(type), kUnifiedTrayIconSize,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary))));
  }
  UpdateLabelOrImageViewColor(/*active=*/false);

  auto* shell = Shell::Get();
  shell->session_controller()->AddObserver(this);
  shell->media_controller()->AddObserver(this);

  SetVisible(false);
}

CameraMicTrayItemView::~CameraMicTrayItemView() {
  auto* shell = Shell::Get();
  shell->media_controller()->RemoveObserver(this);
  shell->session_controller()->RemoveObserver(this);
}

void CameraMicTrayItemView::OnVmMediaNotificationChanged(bool camera,
                                                         bool mic,
                                                         bool camera_and_mic) {
  switch (type_) {
    case Type::kCamera:
      active_ = camera || camera_and_mic;
      with_mic_ = camera_and_mic;
      FetchMessage();
      break;
    case Type::kMic:
      active_ = mic;
      break;
  }
  Update();
}

const char* CameraMicTrayItemView::GetClassName() const {
  return "CameraMicTrayItemView";
}

void CameraMicTrayItemView::Update() {
  // Hide for non-primary session because we only show the indicators for VMs
  // for now, and VMs support only the primary session.
  SetVisible(active_ && is_primary_session_);
}

std::u16string CameraMicTrayItemView::GetAccessibleNameString() const {
  return message_;
}

views::View* CameraMicTrayItemView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

std::u16string CameraMicTrayItemView::GetTooltipText(
    const gfx::Point& p) const {
  return message_;
}

void CameraMicTrayItemView::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  is_primary_session_ = Shell::Get()->session_controller()->IsUserPrimary();
  Update();
}

void CameraMicTrayItemView::HandleLocaleChange() {
  FetchMessage();
}

void CameraMicTrayItemView::UpdateLabelOrImageViewColor(bool active) {
  if (!chromeos::features::IsJellyEnabled()) {
    return;
  }
  TrayItemView::UpdateLabelOrImageViewColor(active);

  image_view()->SetImage(ui::ImageModel::FromVectorIcon(
      GetIcon(type_),
      active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
             : cros_tokens::kCrosSysOnSurface,
      kUnifiedTrayIconSize));
}

void CameraMicTrayItemView::FetchMessage() {
  switch (type_) {
    case Type::kCamera:
      message_ = l10n_util::GetStringUTF16(
          with_mic_ ? IDS_ASH_CAMERA_MIC_VM_USING_CAMERA_AND_MIC
                    : IDS_ASH_CAMERA_MIC_VM_USING_CAMERA);
      break;
    case Type::kMic:
      message_ = l10n_util::GetStringUTF16(IDS_ASH_CAMERA_MIC_VM_USING_MIC);
      break;
  }
}

const gfx::VectorIcon& CameraMicTrayItemView::GetIcon(Type type) {
  switch (type_) {
    case Type::kCamera:
      return ::vector_icons::kVideocamIcon;
    case Type::kMic:
      return ::vector_icons::kMicIcon;
  }
}

}  // namespace ash

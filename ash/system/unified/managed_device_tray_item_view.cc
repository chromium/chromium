// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/managed_device_tray_item_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

ManagedDeviceTrayItemView::ManagedDeviceTrayItemView(Shelf* shelf)
    : TrayItemView(shelf) {
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->system_tray_model()->enterprise_domain()->AddObserver(this);
  CreateImageView();
  Update();
}

ManagedDeviceTrayItemView::~ManagedDeviceTrayItemView() {
  Shell::Get()->system_tray_model()->enterprise_domain()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void ManagedDeviceTrayItemView::OnLoginStatusChanged(LoginStatus status) {
  Update();
}

void ManagedDeviceTrayItemView::OnDeviceEnterpriseInfoChanged() {
  Update();
}

void ManagedDeviceTrayItemView::OnEnterpriseAccountDomainChanged() {}

void ManagedDeviceTrayItemView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  UpdateIcon();
}

void ManagedDeviceTrayItemView::HandleLocaleChange() {
  UpdateTooltipText();
}

void ManagedDeviceTrayItemView::UpdateLabelOrImageViewColor(bool active) {
  TrayItemView::UpdateLabelOrImageViewColor(active);

  auto* icon = GetIcon();
  if (icon) {
    image_view()->SetImage(ui::ImageModel::FromVectorIcon(
        *icon, active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                      : cros_tokens::kCrosSysOnSurface));
  }
}

void ManagedDeviceTrayItemView::Update() {
  SessionControllerImpl* session = Shell::Get()->session_controller();
  if (!session->IsUserPublicAccount() && !session->IsUserChild()) {
    SetVisible(false);
    return;
  }

  UpdateIcon();
  UpdateTooltipText();
  SetVisible(true);
}

const gfx::VectorIcon* ManagedDeviceTrayItemView::GetIcon() {
  const gfx::VectorIcon* icon = nullptr;
  SessionControllerImpl* session = Shell::Get()->session_controller();
  if (session->IsUserPublicAccount()) {
    icon = &kSystemTrayManagedIcon;
  } else if (session->IsUserChild()) {
    icon = &kSystemTraySupervisedUserIcon;
  }
  return icon;
}

void ManagedDeviceTrayItemView::UpdateIcon() {
  auto* icon = GetIcon();

  if (icon) {
    UpdateLabelOrImageViewColor(is_active());
  }
}

void ManagedDeviceTrayItemView::UpdateTooltipText() {
  SessionControllerImpl* session = Shell::Get()->session_controller();
  if (session->IsUserPublicAccount()) {
    std::string enterprise_domain_manager = Shell::Get()
                                                ->system_tray_model()
                                                ->enterprise_domain()
                                                ->enterprise_domain_manager();
    if (!enterprise_domain_manager.empty()) {
      image_view()->SetTooltipText(l10n_util::GetStringFUTF16(
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY, ui::GetChromeOSDeviceName(),
          base::UTF8ToUTF16(enterprise_domain_manager)));
    } else {
      image_view()->SetTooltipText(std::u16string());
      LOG(WARNING)
          << "Public account user, but device not enterprise-enrolled.";
    }
  } else if (session->IsUserChild()) {
    image_view()->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FAMILY_LINK_LABEL));
  }
}

BEGIN_METADATA(ManagedDeviceTrayItemView)
END_METADATA

}  // namespace ash

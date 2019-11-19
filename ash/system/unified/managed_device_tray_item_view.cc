// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/managed_device_tray_item_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
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

void ManagedDeviceTrayItemView::OnEnterpriseDomainChanged() {
  Update();
}

const char* ManagedDeviceTrayItemView::GetClassName() const {
  return "ManagedDeviceTrayItemView";
}

void ManagedDeviceTrayItemView::Update() {
  SessionControllerImpl* session = Shell::Get()->session_controller();
  if (session->IsUserPublicAccount()) {
    image_view()->SetImage(gfx::CreateVectorIcon(
        kSystemTrayManagedIcon,
        TrayIconColor(Shell::Get()->session_controller()->GetSessionState())));
    std::string enterprise_domain_name = Shell::Get()
                                             ->system_tray_model()
                                             ->enterprise_domain()
                                             ->enterprise_display_domain();
    SetVisible(true);
    if (!enterprise_domain_name.empty()) {
      image_view()->set_tooltip_text(l10n_util::GetStringFUTF16(
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY,
          base::UTF8ToUTF16(enterprise_domain_name)));
    } else {
      image_view()->set_tooltip_text(base::string16());
      LOG(WARNING)
          << "Public account user, but device not enterprise-enrolled.";
    }
  } else if (session->IsUserChild()) {
    image_view()->SetImage(gfx::CreateVectorIcon(
        kSystemTrayFamilyLinkIcon,
        TrayIconColor(Shell::Get()->session_controller()->GetSessionState())));
    image_view()->set_tooltip_text(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FAMILY_LINK_LABEL));
    SetVisible(true);
  } else {
    SetVisible(false);
  }
}

}  // namespace ash

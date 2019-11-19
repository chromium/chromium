// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_managed_device_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/supervised/supervised_icon_string.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

UnifiedManagedDeviceView::UnifiedManagedDeviceView()
    : icon_(new views::ImageView), label_(new views::Label) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      kUnifiedManagedDeviceViewPadding, kUnifiedManagedDeviceSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  icon_->SetPreferredSize(
      gfx::Size(kUnifiedSystemInfoHeight, kUnifiedSystemInfoHeight));
  AddChildView(icon_);

  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetSubpixelRenderingEnabled(false);
  label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextSecondary,
      AshColorProvider::AshColorMode::kDark));
  AddChildView(label_);

  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->system_tray_model()->enterprise_domain()->AddObserver(this);
  Update();
}

UnifiedManagedDeviceView::~UnifiedManagedDeviceView() {
  Shell::Get()->system_tray_model()->enterprise_domain()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void UnifiedManagedDeviceView::OnLoginStatusChanged(LoginStatus status) {
  Update();
}

void UnifiedManagedDeviceView::OnEnterpriseDomainChanged() {
  Update();
}

const char* UnifiedManagedDeviceView::GetClassName() const {
  return "UnifiedManagedDeviceView";
}

void UnifiedManagedDeviceView::Update() {
  SessionControllerImpl* session = Shell::Get()->session_controller();
  EnterpriseDomainModel* model =
      Shell::Get()->system_tray_model()->enterprise_domain();
  std::string enterprise_domain_name = model->enterprise_display_domain();

  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconSecondary,
      AshColorProvider::AshColorMode::kDark);
  if (session->ShouldDisplayManagedUI() || model->active_directory_managed() ||
      !enterprise_domain_name.empty()) {
    // Show enterpised managed UI.
    icon_->SetImage(gfx::CreateVectorIcon(kSystemTrayManagedIcon, icon_color));

    if (!enterprise_domain_name.empty()) {
      label_->SetText(l10n_util::GetStringFUTF16(
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY,
          base::UTF8ToUTF16(enterprise_domain_name)));
    } else {
      label_->SetText(
          l10n_util::GetStringUTF16(IDS_ASH_ENTERPRISE_DEVICE_MANAGED));
    }

    SetVisible(true);
  } else if (session->IsUserSupervised()) {
    // Show supervised user UI (locally supervised or Family Link).
    icon_->SetImage(gfx::CreateVectorIcon(GetSupervisedUserIcon(), icon_color));
    label_->SetText(GetSupervisedUserMessage());
    SetVisible(true);
  } else {
    SetVisible(false);
  }
}

}  // namespace ash

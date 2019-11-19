// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/tray_ime_chromeos.h"

#include <memory>
#include <vector>

#include "ash/ime/ime_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace tray {

IMEDetailedView::IMEDetailedView(DetailedViewDelegate* delegate,
                                 ImeController* ime_controller)
    : ImeListView(delegate), ime_controller_(ime_controller) {
  DCHECK(ime_controller_);
}

void IMEDetailedView::Update(
    const std::string& current_ime_id,
    const std::vector<mojom::ImeInfo>& list,
    const std::vector<mojom::ImeMenuItem>& property_list,
    bool show_keyboard_toggle,
    SingleImeBehavior single_ime_behavior) {
  ImeListView::Update(current_ime_id, list, property_list, show_keyboard_toggle,
                      single_ime_behavior);
  CreateTitleRow(IDS_ASH_STATUS_TRAY_IME);
}

void IMEDetailedView::ResetImeListView() {
  ImeListView::ResetImeListView();
  settings_button_ = nullptr;
  controlled_setting_icon_ = nullptr;
}

void IMEDetailedView::HandleButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == settings_button_)
    ShowSettings();
  else
    ImeListView::HandleButtonPressed(sender, event);
}

void IMEDetailedView::CreateExtraTitleRowButtons() {
  if (ime_controller_->managed_by_policy()) {
    controlled_setting_icon_ = TrayPopupUtils::CreateMainImageView();
    controlled_setting_icon_->SetImage(gfx::CreateVectorIcon(
        kSystemMenuBusinessIcon,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconPrimary,
            AshColorProvider::AshColorMode::kLight)));
    controlled_setting_icon_->set_tooltip_text(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME_MANAGED));
    tri_view()->AddView(TriView::Container::END, controlled_setting_icon_);
  }

  tri_view()->SetContainerVisible(TriView::Container::END, true);
  settings_button_ = CreateSettingsButton(IDS_ASH_STATUS_TRAY_IME_SETTINGS);
  tri_view()->AddView(TriView::Container::END, settings_button_);
}

void IMEDetailedView::ShowSettings() {
  base::RecordAction(base::UserMetricsAction("StatusArea_IME_Detailed"));
  CloseBubble();  // Deletes |this|.
  Shell::Get()->system_tray_model()->client()->ShowIMESettings();
}

const char* IMEDetailedView::GetClassName() const {
  return "IMEDetailedView";
}

}  // namespace tray

}  // namespace ash

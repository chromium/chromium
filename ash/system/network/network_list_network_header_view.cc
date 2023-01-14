// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_network_header_view.h"

#include "ash/ash_export.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_list_header_view.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/weak_ptr.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

NetworkListNetworkHeaderView::NetworkListNetworkHeaderView(
    Delegate* delegate,
    int label_id,
    const gfx::VectorIcon& vector_icon)
    : NetworkListHeaderView(label_id),
      model_(Shell::Get()->system_tray_model()->network_state_model()),
      delegate_(delegate) {
  std::unique_ptr<TrayToggleButton> toggle = std::make_unique<TrayToggleButton>(
      base::BindRepeating(&NetworkListNetworkHeaderView::ToggleButtonPressed,
                          weak_factory_.GetWeakPtr()),
      label_id);
  toggle->SetID(kToggleButtonId);
  toggle_ = toggle.get();
  container()->AddView(TriView::Container::END, toggle.release());
  if (features::IsQsRevampEnabled()) {
    auto image_view = std::make_unique<views::ImageView>();
    image_view->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icon, cros_tokens::kCrosSysOnSurface));
    image_view->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 10, 0, 0));
    container()->AddView(TriView::Container::START, image_view.release());
  }
}

NetworkListNetworkHeaderView::~NetworkListNetworkHeaderView() = default;

void NetworkListNetworkHeaderView::SetToggleState(bool enabled,
                                                  bool is_on,
                                                  bool animate_toggle) {
  toggle_->SetEnabled(enabled);
  toggle_->SetAcceptsEvents(enabled);

  if (animate_toggle) {
    toggle_->AnimateIsOn(is_on);
    return;
  }

  toggle_->SetIsOn(is_on);
}

void NetworkListNetworkHeaderView::AddExtraButtons() {}

void NetworkListNetworkHeaderView::OnToggleToggled(bool is_on) {}

void NetworkListNetworkHeaderView::SetToggleVisibility(bool visible) {
  toggle_->SetVisible(visible);
}

void NetworkListNetworkHeaderView::ToggleButtonPressed() {
  // In the event of frequent clicks, helps to prevent a toggle button state
  // from becoming inconsistent with the async operation of enabling /
  // disabling of mobile radio. The toggle will get unlocked in the next
  // call to SetToggleState(). Note that we don't disable/enable
  // because that would clear focus.
  toggle_->SetAcceptsEvents(false);
  OnToggleToggled(toggle_->GetIsOn());
}

}  // namespace ash

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_network_header_view.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/switch.h"
#include "ash/style/typography.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_list_header_view.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

NetworkListNetworkHeaderView::NetworkListNetworkHeaderView(
    Delegate* delegate,
    int label_id,
    const gfx::VectorIcon& vector_icon)
    : NetworkListHeaderView(),
      model_(Shell::Get()->system_tray_model()->network_state_model()),
      enabled_label_id_(label_id),
      delegate_(delegate) {
  auto toggle = std::make_unique<Switch>(
      base::BindRepeating(&NetworkListNetworkHeaderView::ToggleButtonPressed,
                          weak_factory_.GetWeakPtr()));
  toggle->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(label_id));
  toggle->SetID(kToggleButtonId);
  toggle_ = toggle.get();
  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icon, cros_tokens::kCrosSysOnSurface));
  entry_row()->AddViewAndLabel(std::move(image_view),
                               l10n_util::GetStringUTF16(label_id));
  // The tooltip provides a better accessibility label than the default
  // provided by AddViewAndLabel() above.
  entry_row()->GetViewAccessibility().SetName(u"");
  entry_row()->text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  ash::TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton1,
                                             *entry_row()->text_label());
  entry_row()->SetExpandable(true);
  entry_row()->AddRightView(toggle.release());
  // ChromeVox users will use the `toggle_` to toggle the feature.
  entry_row()->left_view()->GetViewAccessibility().SetIsIgnored(true);
  entry_row()->text_label()->GetViewAccessibility().SetIsIgnored(true);
}

NetworkListNetworkHeaderView::~NetworkListNetworkHeaderView() = default;

void NetworkListNetworkHeaderView::SetToggleState(bool enabled,
                                                  bool is_on,
                                                  bool animate_toggle) {
  entry_row()->SetEnabled(enabled);
  toggle_->SetEnabled(enabled);
  toggle_->SetAcceptsEvents(enabled);
  if (animate_toggle) {
    toggle_->AnimateIsOn(is_on);
    return;
  }

  toggle_->SetIsOn(is_on);
}

void NetworkListNetworkHeaderView::OnToggleToggled(bool is_on) {}

void NetworkListNetworkHeaderView::SetToggleVisibility(bool visible) {
  toggle_->SetVisible(visible);
}

void NetworkListNetworkHeaderView::ToggleButtonPressed() {
  // The `toggle_` is pressed directly, it has the new state when calling
  // `GetIsOn()`.
  UpdateToggleState(/*has_new_state=*/true);
}

void NetworkListNetworkHeaderView::UpdateToggleState(bool has_new_state) {
  // In the event of frequent clicks, helps to prevent a toggle button state
  // from becoming inconsistent with the async operation of enabling /
  // disabling of mobile radio. The toggle will get unlocked in the next
  // call to SetToggleState(). Note that we don't disable/enable
  // because that would clear focus.
  toggle_->SetAcceptsEvents(false);
  OnToggleToggled(has_new_state ? toggle_->GetIsOn() : !toggle_->GetIsOn());
}

BEGIN_METADATA(NetworkListNetworkHeaderView)
END_METADATA

}  // namespace ash

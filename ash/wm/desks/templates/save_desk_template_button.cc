// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/save_desk_template_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

constexpr int kCornerRadius = 16;

SaveDeskTemplateButton::SaveDeskTemplateButton(base::RepeatingClosure callback)
    : PillButton(callback,
                 l10n_util::GetStringUTF16(
                     IDS_ASH_DESKS_TEMPLATES_SAVE_DESK_AS_TEMPLATE_BUTTON),
                 PillButton::Type::kIcon,
                 &kSaveDeskAsTemplateIcon),
      callback_(callback) {
  SetBorder(std::make_unique<WmHighlightItemBorder>(kCornerRadius));
  SetBackgroundColor(AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80));
}

SaveDeskTemplateButton::~SaveDeskTemplateButton() = default;

views::View* SaveDeskTemplateButton::GetView() {
  return this;
}

void SaveDeskTemplateButton::MaybeActivateHighlightedView() {
  callback_.Run();
}

void SaveDeskTemplateButton::MaybeCloseHighlightedView() {}

void SaveDeskTemplateButton::MaybeSwapHighlightedView(bool right) {}

void SaveDeskTemplateButton::OnViewHighlighted() {
  UpdateBorderState();
}

void SaveDeskTemplateButton::OnViewUnhighlighted() {
  UpdateBorderState();
}

void SaveDeskTemplateButton::UpdateBorderState() {
  auto* border = static_cast<WmHighlightItemBorder*>(GetBorder());
  border->SetFocused(IsViewHighlighted());
  SchedulePaint();
}

BEGIN_METADATA(SaveDeskTemplateButton, PillButton)
END_METADATA

}  // namespace ash

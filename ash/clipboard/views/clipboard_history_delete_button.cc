// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_delete_button.h"

#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"

namespace ash {

ClipboardHistoryDeleteButton::ClipboardHistoryDeleteButton(
    ClipboardHistoryItemView* listener)
    : CloseButton(
          base::BindRepeating(
              [](ClipboardHistoryItemView* item_view, const ui::Event& event) {
                item_view->HandleDeleteButtonPressEvent(event);
              },
              base::Unretained(listener)),
          CloseButton::Type::kSmall,
          /*icon=*/nullptr,
          kColorAshShieldAndBase80Light,
          kColorAshButtonIconColorLight),
      listener_(listener) {
  SetID(clipboard_history_util::kDeleteButtonViewID);
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_DELETE_BUTTON));
  SetTooltipText(l10n_util::GetStringUTF16(
      IDS_CLIPBOARD_HISTORY_DELETE_BUTTON_HOVER_TEXT));
  SetVisible(false);
  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/true);
}

ClipboardHistoryDeleteButton::~ClipboardHistoryDeleteButton() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/RemoveLayerBeneathView(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

void ClipboardHistoryDeleteButton::AddLayerBeneathView(ui::Layer* layer) {
  ink_drop_container_->AddLayerBeneathView(layer);
}

void ClipboardHistoryDeleteButton::OnClickCanceled(const ui::Event& event) {
  DCHECK(event.IsMouseEvent());

  listener_->OnMouseClickOnDescendantCanceled();
  views::Button::OnClickCanceled(event);
}

void ClipboardHistoryDeleteButton::RemoveLayerBeneathView(ui::Layer* layer) {
  ink_drop_container_->RemoveLayerBeneathView(layer);
}

BEGIN_METADATA(ClipboardHistoryDeleteButton, CloseButton)
END_METADATA

}  // namespace ash

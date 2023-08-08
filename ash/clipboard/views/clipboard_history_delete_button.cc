// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_delete_button.h"

#include <string>

#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/style_util.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"

namespace ash {

ClipboardHistoryDeleteButton::ClipboardHistoryDeleteButton(
    ClipboardHistoryItemView* listener,
    const std::u16string& item_text)
    : CloseButton(
          base::BindRepeating(
              [](ClipboardHistoryItemView* item_view, const ui::Event& event) {
                item_view->HandleDeleteButtonPressEvent(event);
              },
              base::Unretained(listener)),
          chromeos::features::IsClipboardHistoryRefreshEnabled()
              // When the refresh is enabled, we will replace the `CloseButton`
              // icon with our own; specify that the `CloseButton` icon floats
              // so that no background is set.
              ? CloseButton::Type::kMediumFloating
              : CloseButton::Type::kSmall,
          /*icon=*/nullptr,
          cros_tokens::kCrosSysSurface,
          cros_tokens::kCrosSysSecondary),
      listener_(listener) {
  SetID(clipboard_history_util::kDeleteButtonViewID);
  SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_CLIPBOARD_HISTORY_DELETE_ITEM_TEXT, item_text));
  SetTooltipText(l10n_util::GetStringUTF16(
      IDS_CLIPBOARD_HISTORY_DELETE_BUTTON_HOVER_TEXT));
  SetVisible(false);

  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      kRemoveOutlineIcon, cros_tokens::kCrosSysSecondary,
                      ClipboardHistoryViews::kDeleteButtonIconSize));
  }

  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/true);
}

ClipboardHistoryDeleteButton::~ClipboardHistoryDeleteButton() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/RemoveLayerFromRegions(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

void ClipboardHistoryDeleteButton::AddLayerToRegion(ui::Layer* layer,
                                                    views::LayerRegion region) {
  ink_drop_container_->AddLayerToRegion(layer, region);
}

void ClipboardHistoryDeleteButton::OnClickCanceled(const ui::Event& event) {
  DCHECK(event.IsMouseEvent());

  listener_->OnMouseClickOnDescendantCanceled();
  views::Button::OnClickCanceled(event);
}

void ClipboardHistoryDeleteButton::RemoveLayerFromRegions(ui::Layer* layer) {
  ink_drop_container_->RemoveLayerFromRegions(layer);
}

BEGIN_METADATA(ClipboardHistoryDeleteButton, CloseButton)
END_METADATA

}  // namespace ash

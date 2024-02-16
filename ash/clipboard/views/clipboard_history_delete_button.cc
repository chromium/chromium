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
#include "base/functional/callback_helpers.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/rect_based_targeting_utils.h"

namespace ash {
namespace {

// Helpers ---------------------------------------------------------------------

std::reference_wrapper<const gfx::VectorIcon> GetIcon() {
  return chromeos::features::IsClipboardHistoryRefreshEnabled()
             ? kRemoveOutlineIcon
             : kSmallCloseButtonIcon;
}

int GetIconSize() {
  return chromeos::features::IsClipboardHistoryRefreshEnabled()
             ? ClipboardHistoryViews::kDeleteButtonV2IconSize
             : ClipboardHistoryViews::kDeleteButtonIconSize;
}

gfx::Size GetSize() {
  return chromeos::features::IsClipboardHistoryRefreshEnabled()
             ? ClipboardHistoryViews::kDeleteButtonV2Size
             : ClipboardHistoryViews::kDeleteButtonSize;
}

std::unique_ptr<views::Background> CreateBackground() {
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    return nullptr;
  }
  const gfx::Size size = GetSize();
  return views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSurface, std::min(size.width(), size.height()) / 2);
}

}  // namespace

// ClipboardHistoryDeleteButton ------------------------------------------------

ClipboardHistoryDeleteButton::ClipboardHistoryDeleteButton(
    ClipboardHistoryItemView* listener,
    const std::u16string& item_text)
    : listener_(listener) {
  views::Builder<views::ImageButton>(this)
      .SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_CLIPBOARD_HISTORY_DELETE_ITEM_TEXT, item_text))
      .SetBackground(CreateBackground())
      .SetCallback(base::BindRepeating(
          &ClipboardHistoryItemView::HandleDeleteButtonPressEvent,
          base::Unretained(listener)))
      .SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY)
      .SetID(clipboard_history_util::kDeleteButtonViewID)
      .SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER)
      .SetImageModel(
          views::Button::STATE_NORMAL,
          ui::ImageModel::FromVectorIcon(
              GetIcon(), cros_tokens::kCrosSysSecondary, GetIconSize()))
      .SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE)
      .SetPaintToLayer()
      .SetPreferredSize(GetSize())
      .SetTooltipText(l10n_util::GetStringUTF16(
          IDS_CLIPBOARD_HISTORY_DELETE_BUTTON_HOVER_TEXT))
      .SetVisible(false)
      .AddChild(views::Builder<views::View>(
                    std::make_unique<views::InkDropContainerView>())
                    .CopyAddressTo(&ink_drop_container_))
      .AfterBuild(base::IgnoreArgs<views::ImageButton*>(base::BindOnce(
          [](ClipboardHistoryDeleteButton* self) {
            self->layer()->SetFillsBoundsOpaquely(false);
            self->SetEventTargeter(std::make_unique<views::ViewTargeter>(self));
            self->SetFocusPainter(nullptr);
            views::InstallCircleHighlightPathGenerator(self);
            StyleUtil::SetUpInkDropForButton(self, gfx::Insets(),
                                             /*highlight_on_hover=*/false,
                                             /*highlight_on_focus=*/true);
          },
          base::Unretained(this))))
      .BuildChildren();
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

void ClipboardHistoryDeleteButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();

  StyleUtil::ConfigureInkDropAttributes(
      this, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity);

  SchedulePaint();
}

void ClipboardHistoryDeleteButton::RemoveLayerFromRegions(ui::Layer* layer) {
  ink_drop_container_->RemoveLayerFromRegions(layer);
}

bool ClipboardHistoryDeleteButton::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  CHECK_EQ(target, this);

  gfx::Rect hit_rect = GetLocalBounds();

  // Only increase the hit_rect for touch events (which have a non-empty
  // bounding box), not for mouse events.
  if (!views::UsePointBasedTargeting(rect)) {
    static constexpr int kMinTouchSize = 20;
    const int outset_h = std::max((kMinTouchSize - hit_rect.width()) / 2, 0);
    const int outset_v = std::max((kMinTouchSize - hit_rect.height()) / 2, 0);
    hit_rect.Outset(gfx::Outsets::VH(outset_v, outset_h));
  }

  return hit_rect.Intersects(rect);
}

BEGIN_METADATA(ClipboardHistoryDeleteButton)
END_METADATA

}  // namespace ash

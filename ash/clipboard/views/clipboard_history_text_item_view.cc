// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_text_item_view.h"

#include <string>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/views/clipboard_history_label.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// TextContentsView

class ClipboardHistoryTextItemView::TextContentsView
    : public ClipboardHistoryTextItemView::ContentsView {
 public:
  METADATA_HEADER(TextContentsView);
  explicit TextContentsView(const std::u16string& text) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* label = AddChildView(std::make_unique<ClipboardHistoryLabel>(text));
    layout->SetFlexForView(label, /*flex=*/1);
    SetClipPath(GetClipPath());
  }
  TextContentsView(const TextContentsView& rhs) = delete;
  TextContentsView& operator=(const TextContentsView& rhs) = delete;
  ~TextContentsView() override = default;

 private:
  // ContentsView:
  SkPath GetClipPath() override {
    if (!chromeos::features::IsClipboardHistoryRefreshEnabled() ||
        !is_delete_button_visible()) {
      return SkPath();
    }

    const float contents_width =
        clipboard_history_util::GetPreferredItemViewWidth() -
        ClipboardHistoryViews::kContentsInsets.width() -
        ClipboardHistoryViews::kIconSize.width() -
        ClipboardHistoryViews::kIconMargins.width();

    return SkPathBuilder()
        // Start at the top-left corner.
        .moveTo(0.f, 0.f)
        // Draw a vertical line to the bottom-left corner. Note that this may
        // extend past the height of the text contents, but visually that does
        // not cause a problem.
        .rLineTo(0.f, 40.f)
        // Draw a horizontal line to the bottom-right corner.
        .rLineTo(contents_width, 0.f)
        // Draw a vertical line to the start of the top-right corner's cutout.
        .rLineTo(0.f, -2.f)
        // Draw the top-right corner's cutout.
        .rCubicTo(0.f, -8.f, -6.7f, -10.f, -10.f, -10.f)
        .rLineTo(-4.f, 0.f)
        .rCubicTo(-7.7f, 0.f, -14.f, -6.3f, -14.f, -14.f)
        .rLineTo(0.f, -4.f)
        .rCubicTo(0.f, -3.3f, -2.f, -10.f, -10.f, -10.f)
        // Draw a horizontal line back to the starting point.
        .lineTo(0.f, 0.f)
        .close()
        .detach();
  }
};

BEGIN_METADATA(ClipboardHistoryTextItemView, TextContentsView, ContentsView)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryTextItemView

ClipboardHistoryTextItemView::ClipboardHistoryTextItemView(
    const base::UnguessableToken& item_id,
    const ClipboardHistory* clipboard_history,
    views::MenuItemView* container)
    : ClipboardHistoryItemView(item_id, clipboard_history, container),
      text_(GetClipboardHistoryItem()->display_text()) {
  SetAccessibleName(text_);
}

ClipboardHistoryTextItemView::~ClipboardHistoryTextItemView() = default;

std::unique_ptr<ClipboardHistoryTextItemView::ContentsView>
ClipboardHistoryTextItemView::CreateContentsView() {
  return std::make_unique<TextContentsView>(text_);
}

BEGIN_METADATA(ClipboardHistoryTextItemView, ClipboardHistoryItemView)
END_METADATA

}  // namespace ash

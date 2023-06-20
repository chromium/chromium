// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_text_item_view.h"

#include <string>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/views/clipboard_history_label.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

////////////////////////////////////////////////////////////////////////////////
// TextContentsView

class TextContentsView : public views::View {
 public:
  METADATA_HEADER(TextContentsView);
  explicit TextContentsView(const std::u16string& text) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* label = AddChildView(std::make_unique<ClipboardHistoryLabel>(text));
    layout->SetFlexForView(label, /*flex=*/1);
  }
  TextContentsView(const TextContentsView& rhs) = delete;
  TextContentsView& operator=(const TextContentsView& rhs) = delete;
  ~TextContentsView() override = default;
};

BEGIN_METADATA(TextContentsView, views::View)
END_METADATA

}  // namespace

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

std::unique_ptr<views::View>
ClipboardHistoryTextItemView::CreateContentsView() {
  return std::make_unique<TextContentsView>(text_);
}

BEGIN_METADATA(ClipboardHistoryTextItemView, ClipboardHistoryItemView)
END_METADATA

}  // namespace ash

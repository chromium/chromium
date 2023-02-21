// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_text_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/views/clipboard_history_delete_button.h"
#include "ash/clipboard/views/clipboard_history_label.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryTextItemView::TextContentsView

class ClipboardHistoryTextItemView::TextContentsView
    : public ClipboardHistoryTextItemView::ContentsView {
 public:
  METADATA_HEADER(TextContentsView);
  explicit TextContentsView(ClipboardHistoryTextItemView* container)
      : ContentsView(container) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* label =
        AddChildView(std::make_unique<ClipboardHistoryLabel>(container->text_));
    layout->SetFlexForView(label, /*flex_weight=*/1);

    InstallDeleteButton();
  }
  TextContentsView(const TextContentsView& rhs) = delete;
  TextContentsView& operator=(const TextContentsView& rhs) = delete;
  ~TextContentsView() override = default;

 private:
  // ContentsView:
  ClipboardHistoryDeleteButton* CreateDeleteButton() override {
    auto delete_button =
        std::make_unique<ClipboardHistoryDeleteButton>(container());
    delete_button->SetProperty(
        views::kMarginsKey,
        ClipboardHistoryViews::kDefaultItemDeleteButtonMargins);
    return AddChildView(std::move(delete_button));
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
  return std::make_unique<TextContentsView>(this);
}

BEGIN_METADATA(ClipboardHistoryTextItemView, ClipboardHistoryItemView)
END_METADATA

}  // namespace ash

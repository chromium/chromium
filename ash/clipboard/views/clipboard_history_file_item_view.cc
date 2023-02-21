// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_file_item_view.h"

#include <array>

#include "ash/clipboard/clipboard_history_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"

namespace {

// The file icon's preferred size.
constexpr gfx::Size kIconSize(20, 20);

// The file icon's margin.
constexpr auto kIconMargin = gfx::Insets::TLBR(0, 0, 0, 12);
}  // namespace

namespace ash {

ClipboardHistoryFileItemView::ClipboardHistoryFileItemView(
    const base::UnguessableToken& item_id,
    const ClipboardHistory* clipboard_history,
    views::MenuItemView* container)
    : ClipboardHistoryTextItemView(item_id, clipboard_history, container) {}
ClipboardHistoryFileItemView::~ClipboardHistoryFileItemView() = default;

std::unique_ptr<ClipboardHistoryFileItemView::ContentsView>
ClipboardHistoryFileItemView::CreateContentsView() {
  auto contents_view = ClipboardHistoryTextItemView::CreateContentsView();

  // `file_icon` should be `contents_view`'s first child.
  if (const auto* item = GetClipboardHistoryItem()) {
    views::ImageView* file_icon = contents_view->AddChildViewAt(
        std::make_unique<views::ImageView>(), /*index=*/0);
    file_icon->SetImageSize(kIconSize);
    file_icon->SetProperty(views::kMarginsKey, kIconMargin);
    file_icon->SetImage(clipboard_history_util::GetIconForFileClipboardItem(
        item, base::UTF16ToUTF8(text())));
  }

  return contents_view;
}

BEGIN_METADATA(ClipboardHistoryFileItemView, ClipboardHistoryTextItemView)
END_METADATA

}  // namespace ash

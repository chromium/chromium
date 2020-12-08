// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_file_item_view.h"

#include "ash/public/cpp/file_icon_util.h"
#include "ash/style/ash_color_provider.h"
#include "base/files/file_path.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"

namespace {

// The file icon's preferred size.
constexpr gfx::Size kIconSize(20, 20);

// The file icon's margin.
constexpr gfx::Insets kIconMargin(/*top=*/0,
                                  /*left=*/0,
                                  /*bottom=*/0,
                                  /*right=*/12);
}  // namespace

namespace ash {

ClipboardHistoryFileItemView::ClipboardHistoryFileItemView(
    const ClipboardHistoryItem* clipboard_history_item,
    views::MenuItemView* container)
    : ClipboardHistoryTextItemView(clipboard_history_item, container) {}
ClipboardHistoryFileItemView::~ClipboardHistoryFileItemView() = default;

std::unique_ptr<ClipboardHistoryFileItemView::ContentsView>
ClipboardHistoryFileItemView::CreateContentsView() {
  auto file_icon = std::make_unique<views::ImageView>();
  const std::string copied_file_name = base::UTF16ToUTF8(text());
  file_icon->SetImage(GetIconForPath(
      base::FilePath(copied_file_name),
      ash::AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  file_icon->SetImageSize(kIconSize);
  file_icon->SetProperty(views::kMarginsKey, kIconMargin);
  auto contents_view = ClipboardHistoryTextItemView::CreateContentsView();

  // `file_icon` should be `contents_view`'s first child.
  contents_view->AddChildViewAt(std::move(file_icon), /*index=*/0);

  return contents_view;
}

const char* ClipboardHistoryFileItemView::GetClassName() const {
  return "ClipboardHistoryFileItemView";
}

}  // namespace ash

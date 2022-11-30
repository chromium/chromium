// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_TEXT_ITEM_VIEW_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_TEXT_ITEM_VIEW_H_

#include "ash/clipboard/views/clipboard_history_item_view.h"

namespace views {
class MenuItemView;
}  // namespace views

namespace ash {

// The menu item showing the plain text.
class ClipboardHistoryTextItemView : public ClipboardHistoryItemView {
 public:
  ClipboardHistoryTextItemView(
      const ClipboardHistoryItem* clipboard_history_item,
      views::MenuItemView* container);
  ClipboardHistoryTextItemView(const ClipboardHistoryTextItemView& rhs) =
      delete;
  ClipboardHistoryItemView& operator=(const ClipboardHistoryTextItemView& rhs) =
      delete;
  ~ClipboardHistoryTextItemView() override;

 protected:
  const std::u16string& text() const { return text_; }

  // ClipboardHistoryItemView:
  std::unique_ptr<ContentsView> CreateContentsView() override;

 private:
  class TextContentsView;

  // ClipboardHistoryItemView:
  std::u16string GetAccessibleName() const override;
  const char* GetClassName() const override;

  // Text to show.
  const std::u16string text_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_TEXT_ITEM_VIEW_H_

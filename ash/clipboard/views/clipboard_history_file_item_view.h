// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_FILE_ITEM_VIEW_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_FILE_ITEM_VIEW_H_

#include "ash/clipboard/views/clipboard_history_text_item_view.h"
#include "base/unguessable_token.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class MenuItemView;
}

namespace ash {
class ClipboardHistory;

// The menu item showing the copied file.
class ClipboardHistoryFileItemView : public ClipboardHistoryTextItemView {
 public:
  METADATA_HEADER(ClipboardHistoryFileItemView);
  ClipboardHistoryFileItemView(const base::UnguessableToken& item_id,
                               const ClipboardHistory* clipboard_history,
                               views::MenuItemView* container);
  ClipboardHistoryFileItemView(const ClipboardHistoryFileItemView& rhs) =
      delete;
  ClipboardHistoryFileItemView& operator=(
      const ClipboardHistoryFileItemView& rhs) = delete;
  ~ClipboardHistoryFileItemView() override;

 private:
  // ClipboardHistoryTextItemView:
  std::unique_ptr<ContentsView> CreateContentsView() override;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_FILE_ITEM_VIEW_H_

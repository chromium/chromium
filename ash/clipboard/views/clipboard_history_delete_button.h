// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_DELETE_BUTTON_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_DELETE_BUTTON_H_

#include "ash/style/close_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class InkDropContainerView;
}  // namespace views

namespace ash {
class ClipboardHistoryItemView;

// The button to delete the menu item and its corresponding clipboard data.
class ClipboardHistoryDeleteButton : public CloseButton {
 public:
  METADATA_HEADER(ClipboardHistoryDeleteButton);

  explicit ClipboardHistoryDeleteButton(ClipboardHistoryItemView* listener);
  ClipboardHistoryDeleteButton(const ClipboardHistoryDeleteButton& rhs) =
      delete;
  ClipboardHistoryDeleteButton& operator=(
      const ClipboardHistoryDeleteButton& rhs) = delete;
  ~ClipboardHistoryDeleteButton() override;

 private:
  // views::ImageButton:
  void AddLayerBeneathView(ui::Layer* layer) override;
  void OnClickCanceled(const ui::Event& event) override;
  void RemoveLayerBeneathView(ui::Layer* layer) override;

  // Used to accommodate the ink drop layer. It ensures that the ink drop is
  // above the view background.
  views::InkDropContainerView* ink_drop_container_ = nullptr;

  // The listener of button events.
  ClipboardHistoryItemView* const listener_;
};
}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_DELETE_BUTTON_H_

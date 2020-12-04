// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_DELETE_BUTTON_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_DELETE_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

namespace views {
class InkDropContainerView;
}  // namespace views

namespace ash {
class ClipboardHistoryItemView;

// The button to delete the menu item and its corresponding clipboard data.
class ClipboardHistoryDeleteButton : public views::ImageButton {
 public:
  explicit ClipboardHistoryDeleteButton(ClipboardHistoryItemView* listener);
  ClipboardHistoryDeleteButton(const ClipboardHistoryDeleteButton& rhs) =
      delete;
  ClipboardHistoryDeleteButton& operator=(
      const ClipboardHistoryDeleteButton& rhs) = delete;
  ~ClipboardHistoryDeleteButton() override;

 private:
  // views::ImageButton:
  const char* GetClassName() const override;
  void AddLayerBeneathView(ui::Layer* layer) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  void OnThemeChanged() override;
  void RemoveLayerBeneathView(ui::Layer* layer) override;

  // Used to accommodate the ink drop layer. It ensures that the ink drop is
  // above the view background.
  views::InkDropContainerView* ink_drop_container_ = nullptr;
};
}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_DELETE_BUTTON_H_

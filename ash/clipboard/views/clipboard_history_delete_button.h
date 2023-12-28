// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_DELETE_BUTTON_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_DELETE_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {
class ClipboardHistoryItemView;

// The button to delete the menu item and its corresponding clipboard data.
class ClipboardHistoryDeleteButton : public views::ImageButton,
                                     public views::ViewTargeterDelegate {
  METADATA_HEADER(ClipboardHistoryDeleteButton, views::ImageButton)

 public:
  ClipboardHistoryDeleteButton(ClipboardHistoryItemView* listener,
                               const std::u16string& item_text);
  ClipboardHistoryDeleteButton(const ClipboardHistoryDeleteButton& rhs) =
      delete;
  ClipboardHistoryDeleteButton& operator=(
      const ClipboardHistoryDeleteButton& rhs) = delete;
  ~ClipboardHistoryDeleteButton() override;

 private:
  // views::ImageButton:
  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override;
  void OnClickCanceled(const ui::Event& event) override;
  void OnThemeChanged() override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // Used to accommodate the ink drop layer. It ensures that the ink drop is
  // above the view background.
  raw_ptr<views::View> ink_drop_container_ = nullptr;

  // The listener of button events.
  const raw_ptr<ClipboardHistoryItemView> listener_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_DELETE_BUTTON_H_

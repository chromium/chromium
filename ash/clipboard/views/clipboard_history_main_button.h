// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_MAIN_BUTTON_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_MAIN_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {
class ClipboardHistoryItemView;

// The view responding to mouse click or gesture tap events.
class ClipboardHistoryMainButton : public views::Button {
 public:
  METADATA_HEADER(ClipboardHistoryMainButton);
  explicit ClipboardHistoryMainButton(ClipboardHistoryItemView* container);
  ClipboardHistoryMainButton(const ClipboardHistoryMainButton& rhs) = delete;
  ClipboardHistoryMainButton& operator=(const ClipboardHistoryMainButton& rhs) =
      delete;
  ~ClipboardHistoryMainButton() override;

  void OnHostPseudoFocusUpdated();

 private:
  void SetShouldHighlight(bool should_highlight);

  // views::Button:
  void OnClickCanceled(const ui::Event& event) override;
  void OnThemeChanged() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // The parent view.
  ClipboardHistoryItemView* const container_;

  // Indicates whether the view should be highlighted.
  bool should_highlight_ = false;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_MAIN_BUTTON_H_

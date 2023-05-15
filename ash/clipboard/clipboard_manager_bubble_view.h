// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_MANAGER_BUBBLE_VIEW_H_
#define ASH_CLIPBOARD_CLIPBOARD_MANAGER_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

// Contents view class for the clipboard manager widget.
class ASH_EXPORT ClipboardManagerBubbleView
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(ClipboardManagerBubbleView);

  // Returns a pointer to the `ClipboardManagerBubbleView` whose bubble widget
  // has been initialized. Owned by said widget.
  static ClipboardManagerBubbleView* Create(const gfx::Rect& anchor_rect);

  ClipboardManagerBubbleView(const ClipboardManagerBubbleView&) = delete;
  ClipboardManagerBubbleView& operator=(const ClipboardManagerBubbleView&) =
      delete;
  ~ClipboardManagerBubbleView() override;

 private:
  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::u16string GetAccessibleWindowTitle() const override;
  void OnThemeChanged() override;

  explicit ClipboardManagerBubbleView(const gfx::Rect& anchor_rect);
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_MANAGER_BUBBLE_VIEW_H_

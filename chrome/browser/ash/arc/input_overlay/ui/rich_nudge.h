// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_RICH_NUDGE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_RICH_NUDGE_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace arc::input_overlay {
// RichNudge shows up when Game Controls is in the button placement mode. The
// layout follows ash::SystemNudgeView, but the behaviors are controlled by
// DisplayOverlayController.
class RichNudge : public views::BubbleDialogDelegateView {
  METADATA_HEADER(RichNudge, views::BubbleDialogDelegateView)

 public:
  explicit RichNudge(aura::Window* parent_window);

  RichNudge(const RichNudge&) = delete;
  RichNudge& operator=(const RichNudge&) = delete;
  ~RichNudge() override;

  // Flips position to the top-left or bottom-right of the parent window.
  void FlipPosition();

 private:
  friend class RichNudgeTest;

  // views::BubbleDialogDelegate:
  gfx::Rect GetAnchorRect() const override;

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // True if this view is on the top-left of the parent window.
  bool on_top_ = true;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_RICH_NUDGE_H_

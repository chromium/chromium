// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_SCROLL_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_SCROLL_VIEW_H_

#include "ash/accessibility/autoclick/autoclick_controller.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class AutoclickScrollCloseButton;
class AutoclickScrollButton;

// View for the Automatic Clicks scroll bubble, which holds the Automatic Clicks
// scroll menu.
class AutoclickScrollBubbleView : public TrayBubbleView {
  METADATA_HEADER(AutoclickScrollBubbleView, TrayBubbleView)

 public:
  explicit AutoclickScrollBubbleView(TrayBubbleView::InitParams init_params);

  AutoclickScrollBubbleView(const AutoclickScrollBubbleView&) = delete;
  AutoclickScrollBubbleView& operator=(const AutoclickScrollBubbleView&) =
      delete;

  ~AutoclickScrollBubbleView() override;

  // Updates the scroll bubble positioning by updating the |rect| to which the
  // bubble anchors and the |arrow| indicating which side of |rect| it should
  // try to lay out on.
  void UpdateAnchorRect(const gfx::Rect& rect,
                        views::BubbleBorder::Arrow arrow);

  // Updates the scroll bubble's insets. Insets can be set at creation time
  // using TrayBubbleView::InitParams in the constructor and updated at runtime
  // here.
  void UpdateInsets(gfx::Insets insets);

  // TrayBubbleView:
  bool IsAnchoredToStatusArea() const override;
};

// View for the Automatic Clicks scroll menu, which creates and manages
// individual buttons to control Automatic Clicks scrolling.
class AutoclickScrollView : public views::View {
  METADATA_HEADER(AutoclickScrollView, views::View)

 public:
  // Used for testing. Start at 1 because a view IDs should not be 0.
  enum class ButtonId {
    kScrollUp = 1,
    kScrollDown,
    kScrollLeft,
    kScrollRight,
    kCloseScroll,
  };

  // The amount of time to wait during a hover over a scroll pad button before
  // requesting that Autoclick Controller perform a scroll. Visible for tests.
  static constexpr int kAutoclickScrollDelayMs = 50;

  AutoclickScrollView();

  AutoclickScrollView(const AutoclickScrollView&) = delete;
  AutoclickScrollView& operator=(const AutoclickScrollView&) = delete;

  ~AutoclickScrollView() override = default;

 private:
  // views::View:
  void Layout(PassKey) override;

  // Unowned. Owned by views hierarchy.
  raw_ptr<AutoclickScrollButton> scroll_up_button_;
  raw_ptr<AutoclickScrollButton> scroll_down_button_;
  raw_ptr<AutoclickScrollButton> scroll_left_button_;
  raw_ptr<AutoclickScrollButton> scroll_right_button_;
  raw_ptr<AutoclickScrollCloseButton> close_scroll_button_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_SCROLL_VIEW_H_

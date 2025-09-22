// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_widget_delegates.h"

#include "ash/shell.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// WidgetDelegate that is resizable and creates ash's FrameView
// implementation.  This is not in anonymous namespace to access
// WidgetDelegateView's ctor.
class TestWidgetBuilderDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetBuilderDelegate() {
    SetCanFullscreen(true);
    SetCanMaximize(true);
    SetCanMinimize(true);
    SetCanResize(true);
  }
  TestWidgetBuilderDelegate(const TestWidgetBuilderDelegate& other) = delete;
  TestWidgetBuilderDelegate& operator=(const TestWidgetBuilderDelegate& other) =
      delete;
  ~TestWidgetBuilderDelegate() override = default;

  // views::WidgetDelegateView:
  std::unique_ptr<views::FrameView> CreateFrameView(
      views::Widget* widget) override {
    return Shell::Get()->CreateDefaultFrameView(widget);
  }
};

CenteredBubbleDialogModelHost::CenteredBubbleDialogModelHost(
    views::Widget* anchor_widget,
    const gfx::Size& size,
    bool close_on_deactivate)
    : views::BubbleDialogModelHost(ui::DialogModel::Builder().Build(),
                                   /*anchor=*/nullptr,
                                   views::BubbleBorder::Arrow::NONE),
      size_(size) {
  set_parent_window(anchor_widget->GetNativeWindow());
  SetAnchorWidget(anchor_widget);
  set_close_on_deactivate(close_on_deactivate);
  set_desired_bounds_delegate(
      base::BindRepeating(&CenteredBubbleDialogModelHost::GetDesiredBounds,
                          base::Unretained(this)));
}

gfx::Rect CenteredBubbleDialogModelHost::GetDesiredBounds() const {
  if (!anchor_widget()) {
    // Anchor widget may be deleted first.
    return gfx::Rect(size_);
  }
  CHECK(anchor_widget());
  auto centered_bounds = anchor_widget()->GetWindowBoundsInScreen();
  centered_bounds.ToCenteredSize(size_);
  return centered_bounds;
}

views::WidgetDelegate* CreateTestWidgetBuilderDelegate() {
  return new TestWidgetBuilderDelegate();
}

views::test::TestWidgetBuilder CreateWidgetBuilderWithDelegate(
    views::test::WidgetBuilderParams params) {
  views::test::TestWidgetBuilder builder(params);
  builder.SetDelegate(CreateTestWidgetBuilderDelegate());
  return builder;
}

}  // namespace ash

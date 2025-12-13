// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TEST_TEST_FRAME_VIEW_ASH_H_
#define ASH_WM_TEST_TEST_FRAME_VIEW_ASH_H_

#include <memory>

#include "ash/frame/frame_view_ash.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class FrameView;
class Widget;
}  // namespace views

namespace ash {

// A test widget delegate that creates `TestFrameViewAsh` as its frame.
class TestWidgetDelegateAsh : public views::WidgetDelegateView {
 public:
  TestWidgetDelegateAsh();
  TestWidgetDelegateAsh(const TestWidgetDelegateAsh&) = delete;
  TestWidgetDelegateAsh& operator=(const TestWidgetDelegateAsh&) = delete;
  ~TestWidgetDelegateAsh() override;

  // views::WidgetDelegateView:
  std::unique_ptr<views::FrameView> CreateFrameView(
      views::Widget* widget) override;
};

// Support class for testing windows with a maximum size.
class TestFrameViewAsh : public FrameViewAsh {
 public:
  explicit TestFrameViewAsh(views::Widget* widget);
  TestFrameViewAsh(const TestFrameViewAsh&) = delete;
  TestFrameViewAsh& operator=(const TestFrameViewAsh&) = delete;
  ~TestFrameViewAsh() override;

  void SetMaximumSize(const gfx::Size& size);
  void SetMinimumSize(const gfx::Size& size);

  // FrameViewAsh:
  gfx::Size GetMaximumSize() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  gfx::Size maximum_size_;
  gfx::Size minimum_size_;
};

}  // namespace ash

#endif  // ASH_WM_TEST_TEST_FRAME_VIEW_ASH_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TEST_TEST_NON_CLIENT_FRAME_VIEW_ASH_H_
#define ASH_WM_TEST_TEST_NON_CLIENT_FRAME_VIEW_ASH_H_

#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class NonClientFrameView;
class Widget;
}  // namespace views

namespace ash {

// A test widget delegate that creates `TestNonClientFrameViewAsh` as its frame.
class TestWidgetDelegateAsh : public views::WidgetDelegateView {
 public:
  TestWidgetDelegateAsh();
  TestWidgetDelegateAsh(const TestWidgetDelegateAsh&) = delete;
  TestWidgetDelegateAsh& operator=(const TestWidgetDelegateAsh&) = delete;
  ~TestWidgetDelegateAsh() override;

  // views::WidgetDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
};

// Support class for testing windows with a maximum size.
class TestNonClientFrameViewAsh : public NonClientFrameViewAsh {
 public:
  explicit TestNonClientFrameViewAsh(views::Widget* widget);
  TestNonClientFrameViewAsh(const TestNonClientFrameViewAsh&) = delete;
  TestNonClientFrameViewAsh& operator=(const TestNonClientFrameViewAsh&) =
      delete;
  ~TestNonClientFrameViewAsh() override;

  void SetMaximumSize(const gfx::Size& size);
  void SetMinimumSize(const gfx::Size& size);

  // NonClientFrameViewAsh:
  gfx::Size GetMaximumSize() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  gfx::Size maximum_size_;
  gfx::Size minimum_size_;
};

}  // namespace ash

#endif  // ASH_WM_TEST_TEST_NON_CLIENT_FRAME_VIEW_ASH_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/test/test_non_client_frame_view_ash.h"

namespace ash {

TestWidgetDelegateAsh::TestWidgetDelegateAsh() {
  SetHasWindowSizeControls(true);
}

TestWidgetDelegateAsh::~TestWidgetDelegateAsh() {}

std::unique_ptr<views::NonClientFrameView>
TestWidgetDelegateAsh::CreateNonClientFrameView(views::Widget* widget) {
  return std::make_unique<TestNonClientFrameViewAsh>(widget);
}

TestNonClientFrameViewAsh::TestNonClientFrameViewAsh(views::Widget* widget)
    : NonClientFrameViewAsh(widget) {}

TestNonClientFrameViewAsh::~TestNonClientFrameViewAsh() {}

void TestNonClientFrameViewAsh::SetMaximumSize(const gfx::Size& size) {
  maximum_size_ = size;
  frame()->OnSizeConstraintsChanged();
}

void TestNonClientFrameViewAsh::SetMinimumSize(const gfx::Size& size) {
  minimum_size_ = size;
  frame()->OnSizeConstraintsChanged();
}

gfx::Size TestNonClientFrameViewAsh::GetMaximumSize() const {
  return maximum_size_;
}

gfx::Size TestNonClientFrameViewAsh::GetMinimumSize() const {
  return minimum_size_;
}

}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/test/test_frame_view_ash.h"

namespace ash {

TestWidgetDelegateAsh::TestWidgetDelegateAsh() {
  SetHasWindowSizeControls(true);
}

TestWidgetDelegateAsh::~TestWidgetDelegateAsh() {}

std::unique_ptr<views::FrameView> TestWidgetDelegateAsh::CreateFrameView(
    views::Widget* widget) {
  return std::make_unique<TestFrameViewAsh>(widget);
}

TestFrameViewAsh::TestFrameViewAsh(views::Widget* widget)
    : FrameViewAsh(widget) {}

TestFrameViewAsh::~TestFrameViewAsh() {}

void TestFrameViewAsh::SetMaximumSize(const gfx::Size& size) {
  maximum_size_ = size;
  widget()->OnSizeConstraintsChanged();
}

void TestFrameViewAsh::SetMinimumSize(const gfx::Size& size) {
  minimum_size_ = size;
  widget()->OnSizeConstraintsChanged();
}

gfx::Size TestFrameViewAsh::GetMaximumSize() const {
  return maximum_size_;
}

gfx::Size TestFrameViewAsh::GetMinimumSize() const {
  return minimum_size_;
}

}  // namespace ash

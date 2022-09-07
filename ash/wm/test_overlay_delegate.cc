// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/test_overlay_delegate.h"

namespace ash {

TestOverlayDelegate::TestOverlayDelegate() : cancel_count_(0) {}

TestOverlayDelegate::~TestOverlayDelegate() = default;

int TestOverlayDelegate::GetCancelCountAndReset() {
  int count = cancel_count_;
  cancel_count_ = 0;
  return count;
}

void TestOverlayDelegate::Cancel() {
  ++cancel_count_;
}

bool TestOverlayDelegate::IsCancelingKeyEvent(ui::KeyEvent* event) {
  return false;
}

aura::Window* TestOverlayDelegate::GetWindow() {
  return NULL;
}

}  // namespace ash

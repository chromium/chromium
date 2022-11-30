// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_keyboard_controller_observer.h"

#include "base/run_loop.h"

namespace ash {

TestKeyboardControllerObserver::TestKeyboardControllerObserver(
    KeyboardController* controller)
    : controller_(controller) {
  controller_->AddObserver(this);
}

TestKeyboardControllerObserver::~TestKeyboardControllerObserver() = default;

void TestKeyboardControllerObserver::OnKeyboardEnableFlagsChanged(
    const std::set<keyboard::KeyboardEnableFlag>& flags) {
  enable_flags_ = flags;
}

void TestKeyboardControllerObserver::OnKeyboardEnabledChanged(bool enabled) {
  if (!enabled)
    ++destroyed_count_;
}

void TestKeyboardControllerObserver::OnKeyboardConfigChanged(
    const keyboard::KeyboardConfig& config) {
  config_ = config;
}

void TestKeyboardControllerObserver::OnKeyboardVisibilityChanged(bool visible) {
}

void TestKeyboardControllerObserver::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& bounds) {}

void TestKeyboardControllerObserver::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& bounds) {}

void TestKeyboardControllerObserver::OnKeyboardUIDestroyed() {}

}  // namespace ash

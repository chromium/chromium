// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/test/test_activation_delegate.h"

#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace ash {

TestActivationDelegate::TestActivationDelegate() = default;

TestActivationDelegate::TestActivationDelegate(bool activate)
    : activate_(activate) {}

void TestActivationDelegate::SetWindow(aura::Window* window) {
  window_ = window;
  ::wm::SetActivationDelegate(window, this);
  ::wm::SetActivationChangeObserver(window, this);
}

bool TestActivationDelegate::ShouldActivate() const {
  should_activate_count_++;
  return activate_;
}

void TestActivationDelegate::OnWindowActivated(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  DCHECK(window_ == gained_active || window_ == lost_active);
  if (window_ == gained_active) {
    activated_count_++;
  } else if (window_ == lost_active) {
    if (lost_active_count_++ == 0)
      window_was_active_ = wm::IsActiveWindow(window_);
  }
}

}  // namespace ash

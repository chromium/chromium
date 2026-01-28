// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/scoped_app_window.h"

#include <utility>

#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/aura/test/window_destroyed_waiter.h"

namespace ash {

ScopedAppWindow::ScopedAppWindow() = default;

ScopedAppWindow::ScopedAppWindow(extensions::AppWindow* window)
    : window_(window) {}

ScopedAppWindow::ScopedAppWindow(ScopedAppWindow&& other)
    : window_(std::exchange(other.window_, nullptr)) {}

ScopedAppWindow& ScopedAppWindow::operator=(ScopedAppWindow&& other) {
  window_ = std::exchange(other.window_, nullptr);
  return *this;
}

ScopedAppWindow::~ScopedAppWindow() {
  if (window_) {
    aura::test::WindowDestroyedWaiter waiter(window_->GetNativeWindow());
    std::exchange(window_, nullptr)->GetBaseWindow()->Close();
    waiter.Wait();
  }
}

extensions::AppWindow* ScopedAppWindow::Get() {
  return window_.get();
}

void ScopedAppWindow::Reset(extensions::AppWindow* window) {
  window_ = window;
}

extensions::AppWindow* ScopedAppWindow::operator->() {
  return window_.get();
}

}  // namespace ash

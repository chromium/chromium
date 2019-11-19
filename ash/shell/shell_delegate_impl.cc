// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shell_delegate_impl.h"

#include <memory>

#include "ash/accessibility/default_accessibility_delegate.h"
#include "ash/test_screenshot_delegate.h"

namespace ash {
namespace shell {

ShellDelegateImpl::ShellDelegateImpl() = default;

ShellDelegateImpl::~ShellDelegateImpl() = default;

bool ShellDelegateImpl::CanShowWindowForUser(const aura::Window* window) const {
  return true;
}

std::unique_ptr<ash::ScreenshotDelegate>
ShellDelegateImpl::CreateScreenshotDelegate() {
  return std::make_unique<TestScreenshotDelegate>();
}

AccessibilityDelegate* ShellDelegateImpl::CreateAccessibilityDelegate() {
  return new DefaultAccessibilityDelegate;
}

bool ShellDelegateImpl::CanGoBack(gfx::NativeWindow window) const {
  return true;
}

}  // namespace shell
}  // namespace ash

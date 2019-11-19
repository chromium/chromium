// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller_test_api_impl.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"

namespace ash {

namespace {

AccessibilityControllerImpl* GetController() {
  return Shell::Get()->accessibility_controller();
}

}  // namespace

AccessibilityControllerTestApiImpl::AccessibilityControllerTestApiImpl() =
    default;

AccessibilityControllerTestApiImpl::~AccessibilityControllerTestApiImpl() =
    default;

void AccessibilityControllerTestApiImpl::SetLargeCursorEnabled(bool enabled) {
  GetController()->SetLargeCursorEnabled(enabled);
}

bool AccessibilityControllerTestApiImpl::IsLargeCursorEnabled() const {
  return GetController()->large_cursor_enabled();
}

// static
std::unique_ptr<AccessibilityControllerTestApi>
AccessibilityControllerTestApi::Create() {
  return std::make_unique<AccessibilityControllerTestApiImpl>();
}

}  // namespace ash

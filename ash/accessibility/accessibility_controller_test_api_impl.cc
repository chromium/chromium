// Copyright 2019 The Chromium Authors
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
  GetController()->large_cursor().SetEnabled(enabled);
}

bool AccessibilityControllerTestApiImpl::IsLargeCursorEnabled() const {
  return GetController()->large_cursor().enabled();
}

int AccessibilityControllerTestApiImpl::GetDictationSodaDownloadProgress()
    const {
  return GetController()->dictation_soda_download_progress();
}

// static
std::unique_ptr<AccessibilityControllerTestApi>
AccessibilityControllerTestApi::Create() {
  return std::make_unique<AccessibilityControllerTestApiImpl>();
}

}  // namespace ash

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accessibility_focus_ring_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {
AccessibilityFocusRingController* g_instance = nullptr;
}

AccessibilityFocusRingController* AccessibilityFocusRingController::Get() {
  return g_instance;
}

AccessibilityFocusRingController::AccessibilityFocusRingController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AccessibilityFocusRingController::~AccessibilityFocusRingController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash

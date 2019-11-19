// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_TEST_API_IMPL_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_TEST_API_IMPL_H_

#include "ash/public/cpp/test/accessibility_controller_test_api.h"
#include "base/macros.h"

namespace ash {

// Implementation of AccessibilityControllerTestApi.
class AccessibilityControllerTestApiImpl
    : public AccessibilityControllerTestApi {
 public:
  AccessibilityControllerTestApiImpl();
  ~AccessibilityControllerTestApiImpl() override;

  // AccessibilityControllerTestApi:
  void SetLargeCursorEnabled(bool enabled) override;
  bool IsLargeCursorEnabled() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityControllerTestApiImpl);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_TEST_API_IMPL_H_

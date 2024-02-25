// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_TEST_API_IMPL_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_TEST_API_IMPL_H_

#include "ash/public/cpp/test/accessibility_controller_test_api.h"
#include "base/functional/callback_forward.h"

namespace ash {

// Implementation of AccessibilityControllerTestApi.
class AccessibilityControllerTestApiImpl
    : public AccessibilityControllerTestApi {
 public:
  AccessibilityControllerTestApiImpl();
  AccessibilityControllerTestApiImpl(
      const AccessibilityControllerTestApiImpl&) = delete;
  AccessibilityControllerTestApiImpl& operator=(
      const AccessibilityControllerTestApiImpl&) = delete;
  ~AccessibilityControllerTestApiImpl() override;

  // AccessibilityControllerTestApi:
  void SetLargeCursorEnabled(bool enabled) override;
  bool IsLargeCursorEnabled() const override;
  int GetDictationSodaDownloadProgress() const override;
  bool IsDictationKeboardDialogShowing() const override;
  void AcceptDictationKeyboardDialog() override;
  void DismissDictationKeyboardDialog() override;
  void AddShowToastCallbackForTesting(
      base::RepeatingCallback<void(AccessibilityToastType)> callback)
      const override;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_TEST_API_IMPL_H_

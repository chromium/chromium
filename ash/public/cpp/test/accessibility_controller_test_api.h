// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_ACCESSIBILITY_CONTROLLER_TEST_API_H_
#define ASH_PUBLIC_CPP_TEST_ACCESSIBILITY_CONTROLLER_TEST_API_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/functional/callback_forward.h"

namespace ash {

// Test API for AccessibilityController in ash. It is intended for
// integration browser tests that need to verify AccessibilityController
// in ash works as expected.
class ASH_EXPORT AccessibilityControllerTestApi {
 public:
  AccessibilityControllerTestApi() = default;
  virtual ~AccessibilityControllerTestApi() = default;

  static std::unique_ptr<AccessibilityControllerTestApi> Create();

  virtual void SetLargeCursorEnabled(bool enabled) = 0;
  virtual bool IsLargeCursorEnabled() const = 0;
  virtual int GetDictationSodaDownloadProgress() const = 0;
  virtual bool IsDictationKeboardDialogShowing() const = 0;
  virtual void AcceptDictationKeyboardDialog() = 0;
  virtual void DismissDictationKeyboardDialog() = 0;
  virtual void AddShowToastCallbackForTesting(
      base::RepeatingCallback<void(AccessibilityToastType)> callback) const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_ACCESSIBILITY_CONTROLLER_TEST_API_H_

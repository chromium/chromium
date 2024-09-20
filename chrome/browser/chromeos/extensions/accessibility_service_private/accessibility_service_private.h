// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ACCESSIBILITY_SERVICE_PRIVATE_ACCESSIBILITY_SERVICE_PRIVATE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ACCESSIBILITY_SERVICE_PRIVATE_ACCESSIBILITY_SERVICE_PRIVATE_H_

#include "extensions/browser/extension_function.h"

namespace extensions {
class AccessibilityServicePrivateSpeakSelectedTextFunction
    : public ExtensionFunction {
 public:
  AccessibilityServicePrivateSpeakSelectedTextFunction() = default;
  AccessibilityServicePrivateSpeakSelectedTextFunction(
      const AccessibilityServicePrivateSpeakSelectedTextFunction&) = delete;
  const AccessibilityServicePrivateSpeakSelectedTextFunction& operator=(
      const AccessibilityServicePrivateSpeakSelectedTextFunction& other) =
      delete;

 protected:
  ~AccessibilityServicePrivateSpeakSelectedTextFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("accessibilityServicePrivate.speakSelectedText",
                             ACCESSIBILITYSERVICEPRIVATE_SPEAKSELECTEDTEXT)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ACCESSIBILITY_SERVICE_PRIVATE_ACCESSIBILITY_SERVICE_PRIVATE_H_

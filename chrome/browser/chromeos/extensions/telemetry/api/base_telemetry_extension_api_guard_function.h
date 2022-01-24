// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_API_GUARD_FUNCTION_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_API_GUARD_FUNCTION_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace chromeos {

// This class should be a single entry point for all telemetry and diagnostics
// APIs. It verifies whether API functions can be called within caller context
// such as device manufacturer, device model, whether active user is device
// owner.
class BaseTelemetryExtensionApiGuardFunction : public ExtensionFunction {
 public:
  BaseTelemetryExtensionApiGuardFunction();

  BaseTelemetryExtensionApiGuardFunction(
      const BaseTelemetryExtensionApiGuardFunction&) = delete;
  BaseTelemetryExtensionApiGuardFunction& operator=(
      const BaseTelemetryExtensionApiGuardFunction&) = delete;

 protected:
  ~BaseTelemetryExtensionApiGuardFunction() override;

  // ExtensionFunction:
  ResponseAction Run() final;

  bool IsPwaUiOpen();

  void OnGetManufacturer(std::string manufacturer);

  virtual void RunIfAllowed() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_BASE_TELEMETRY_EXTENSION_API_GUARD_FUNCTION_H_

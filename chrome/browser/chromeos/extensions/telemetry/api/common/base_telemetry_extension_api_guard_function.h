// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_BASE_TELEMETRY_EXTENSION_API_GUARD_FUNCTION_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_BASE_TELEMETRY_EXTENSION_API_GUARD_FUNCTION_H_

#include <optional>
#include <string>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/api_guard_delegate.h"
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

  virtual void RunIfAllowed() = 0;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  virtual bool IsCrosApiAvailable() = 0;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 private:
  void OnCanAccessApi(std::optional<std::string> error);

  std::unique_ptr<ApiGuardDelegate> api_guard_delegate_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_BASE_TELEMETRY_EXTENSION_API_GUARD_FUNCTION_H_

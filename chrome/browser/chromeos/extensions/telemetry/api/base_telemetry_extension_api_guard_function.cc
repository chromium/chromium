// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_api_guard_function.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/api_guard_delegate.h"

namespace chromeos {

BaseTelemetryExtensionApiGuardFunction::
    BaseTelemetryExtensionApiGuardFunction() = default;
BaseTelemetryExtensionApiGuardFunction::
    ~BaseTelemetryExtensionApiGuardFunction() = default;

ExtensionFunction::ResponseAction
BaseTelemetryExtensionApiGuardFunction::Run() {
  // ExtensionFunction::Run() can be expected to run at most once for the
  // lifetime of the ExtensionFunction. Therefore, it is safe to instantiate
  // |api_guard_delegate_| here (vs in the ctor).
  api_guard_delegate_ = ApiGuardDelegate::Factory::Create();
  api_guard_delegate_->CanAccessApi(
      browser_context(), extension(),
      base::BindOnce(&BaseTelemetryExtensionApiGuardFunction::OnCanAccessApi,
                     this));

  return RespondLater();
}

void BaseTelemetryExtensionApiGuardFunction::OnCanAccessApi(std::string error) {
  if (!error.empty()) {
    Respond(Error(base::StringPrintf("Unauthorized access to chrome.%s. %s",
                                     name(), error.c_str())));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!IsCrosApiAvailable()) {
    error = "Not implemented.";
    Respond(Error(
        base::StringPrintf("API chrome.%s failed. %s", name(), error.c_str())));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  RunIfAllowed();
}

}  // namespace chromeos

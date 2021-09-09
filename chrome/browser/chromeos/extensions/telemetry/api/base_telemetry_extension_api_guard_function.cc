// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_api_guard_function.h"

#include "base/strings/stringprintf.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_function.h"

namespace chromeos {

BaseTelemetryExtensionApiGuardFunction::
    BaseTelemetryExtensionApiGuardFunction() = default;
BaseTelemetryExtensionApiGuardFunction::
    ~BaseTelemetryExtensionApiGuardFunction() = default;

ExtensionFunction::ResponseAction
BaseTelemetryExtensionApiGuardFunction::Run() {
  if (!user_manager::UserManager::Get()->IsCurrentUserOwner()) {
    return RespondNow(Error(
        base::StringPrintf("Unauthorized access to chrome.%s. "
                           "This extension is not run by the device owner",
                           name())));
  }

  return RunIfAllowed();
}

}  // namespace chromeos

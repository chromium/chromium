// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_api_guard_function.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/hardware_info_delegate.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_function.h"

namespace chromeos {

namespace {

constexpr char kAllowedManufacturer[] = "HP";

}  // namespace

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

  // TODO(b/200676085): figure out a better way to async check different
  // conditions.
  HardwareInfoDelegate::Factory::Create()->GetManufacturer(base::BindOnce(
      &BaseTelemetryExtensionApiGuardFunction::OnGetManufacturer, this));

  return RespondLater();
}

void BaseTelemetryExtensionApiGuardFunction::OnGetManufacturer(
    std::string manufacturer) {
  base::TrimWhitespaceASCII(manufacturer, base::TrimPositions::TRIM_ALL,
                            &manufacturer);

  // TODO(b/200676336): create more general approach to verify manufacturers and
  // extension IDs.
  if (manufacturer != kAllowedManufacturer) {
    Respond(Error(base::StringPrintf(
        "Unauthorized access to chrome.%s. "
        "This extension is not allowed to access the API on this device",
        name())));
    return;
  }

  RunIfAllowed();
}

}  // namespace chromeos

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/system_log/system_log_api.h"

#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/system_log.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/device_event_log/device_event_log.h"
#include "extensions/common/extension.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace extensions {

namespace {

bool IsDeviceExtensionsSystemLogEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool device_extensions_system_log_enabled = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kDeviceExtensionsSystemLogEnabled,
                                       &device_extensions_system_log_enabled);
  return device_extensions_system_log_enabled;
#else
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  return init_params->DeviceSettings()->device_extensions_system_log_enabled ==
         crosapi::mojom::DeviceSettings::OptionalBool::kTrue;
#endif
}

bool IsSigninProfileCheck(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::IsSigninBrowserContext(profile);
#else
  return false;
#endif
}

std::string FormatLogMessage(const std::string& extension_id,
                             Profile* profile,
                             const std::string& message) {
  return base::StringPrintf("[%s]%s: %s", extension_id.c_str(),
                            IsSigninProfileCheck(profile) ? "[signin]" : "",
                            message.c_str());
}

}  // namespace

SystemLogAddFunction::SystemLogAddFunction() = default;
SystemLogAddFunction::~SystemLogAddFunction() = default;

ExtensionFunction::ResponseAction SystemLogAddFunction::Run() {
  auto parameters = api::system_log::Add::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  const api::system_log::MessageOptions& options = parameters->options;

  Profile* profile = Profile::FromBrowserContext(browser_context());

  std::string log_message =
      FormatLogMessage(extension_id(), profile, options.message);
  auto is_outside_user_session =
      chromeos::IsManagedGuestSession() || IsSigninProfileCheck(profile);
  // Only add logs to system log if the policy allows this and we are not in a
  // user session.
  if (IsDeviceExtensionsSystemLogEnabled() && is_outside_user_session) {
    SYSLOG(INFO) << base::StringPrintf("extensions: %s", log_message.c_str());
    // Will not be added to feedback reports to avoid duplication.
    EXTENSIONS_LOG(DEBUG) << log_message;
  } else {
    EXTENSIONS_LOG(EVENT) << log_message;
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions

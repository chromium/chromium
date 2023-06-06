// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_api_guard_function.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/api_guard_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/check.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(user_manager::UserManager::IsInitialized());
  // Wait for the owner manager to fetch the owner. The actual owner `AccountId`
  // is ignored and fetched at a later point. This just makes sure we delay
  // until the owner id is present.
  user_manager::UserManager::Get()->GetOwnerAccountIdAsync(
      base::IgnoreArgs<const AccountId&>(base::BindOnce(
          &BaseTelemetryExtensionApiGuardFunction::InvokeCanAccessApi, this)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  InvokeCanAccessApi();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  return RespondLater();
}

void BaseTelemetryExtensionApiGuardFunction::InvokeCanAccessApi() {
  api_guard_delegate_->CanAccessApi(
      browser_context(), extension(),
      base::BindOnce(&BaseTelemetryExtensionApiGuardFunction::OnCanAccessApi,
                     this));
}

void BaseTelemetryExtensionApiGuardFunction::OnCanAccessApi(
    absl::optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(base::StringPrintf("Unauthorized access to chrome.%s. %s",
                                     name(), error.value().c_str())));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!IsCrosApiAvailable()) {
    const std::string not_implemented_error = "Not implemented.";
    Respond(Error(base::StringPrintf("API chrome.%s failed. %s", name(),
                                     not_implemented_error.c_str())));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  RunIfAllowed();
}

}  // namespace chromeos

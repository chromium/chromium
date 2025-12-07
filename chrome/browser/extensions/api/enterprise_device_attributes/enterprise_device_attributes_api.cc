// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_api.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/policy/core/device_attributes_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/user_manager/user.h"

namespace {

bool IsAccessAllowed() {
  // TODO(crbug.com/354842935): this check looks incorrect, because APIs are
  // running under a certain profile, while this looks at primary user profile.
  // Also GetPrimaryUserProfile has known issue (crbug.com/40227502).
  // We should respect the Profile which can be taken by
  // `Profile::FromBrowserContext(ExtensionFunction::browser_context())`.
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (ash::IsSigninBrowserContext(profile)) {
    return true;
  }
  if (profile->IsOffTheRecord()) {
    return false;
  }

  const user_manager::User* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  if (!user) {
    return false;
  }
  return user->IsAffiliated();
}

}  // namespace

namespace extensions {

EnterpriseDeviceAttributesBase::EnterpriseDeviceAttributesBase()
    : device_attributes_(std::make_unique<policy::DeviceAttributesImpl>()) {}

EnterpriseDeviceAttributesBase::~EnterpriseDeviceAttributesBase() = default;

template <std::invocable F>
  requires std::convertible_to<std::invoke_result_t<F>, std::string>
ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesBase::RespondWithCheck(F&& f) {
  return RespondNow(WithArguments(
      IsAccessAllowed()
          ? f()
          :
          // We intentionally drop the error message here because the
          // extension API is expected to return "" on validation error.
          std::string()));
}

void EnterpriseDeviceAttributesBase::SetDeviceAttributes(
    base::PassKey<EnterpriseDeviceAttributesApiAshTest>,
    std::unique_ptr<policy::DeviceAttributes> device_attributes) {
  device_attributes_ = std::move(device_attributes);
}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::Run() {
  return RespondWithCheck(
      [this]() { return device_attributes().GetDirectoryApiID(); });
}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::Run() {
  return RespondWithCheck(
      [this]() { return device_attributes().GetDeviceSerialNumber(); });
}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceAssetIdFunction::Run() {
  return RespondWithCheck(
      [this]() { return device_attributes().GetDeviceAssetID(); });
}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::Run() {
  return RespondWithCheck(
      [this]() { return device_attributes().GetDeviceAnnotatedLocation(); });
}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceHostnameFunction::Run() {
  return RespondWithCheck([this]() {
    return device_attributes().GetDeviceHostname().value_or(std::string());
  });
}

}  // namespace extensions

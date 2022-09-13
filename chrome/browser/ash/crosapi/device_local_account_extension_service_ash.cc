// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_local_account_extension_service_ash.h"

#include <string>

#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/account_id/account_id.h"
#include "components/crash/core/app/crashpad.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crosapi {
namespace {

absl::optional<std::string> GetPrimaryUserEmail() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (user)
    return user->GetAccountId().GetUserEmail();

  return absl::nullopt;
}

}  // namespace

DeviceLocalAccountExtensionServiceAsh::DeviceLocalAccountExtensionServiceAsh() =
    default;
DeviceLocalAccountExtensionServiceAsh::
    ~DeviceLocalAccountExtensionServiceAsh() = default;

void DeviceLocalAccountExtensionServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DeviceLocalAccountExtensionService>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DeviceLocalAccountExtensionServiceAsh::BindExtensionInstaller(
    mojo::PendingRemote<mojom::DeviceLocalAccountExtensionInstaller>
        installer) {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  absl::optional<std::string> primary_user_email = GetPrimaryUserEmail();
  DCHECK(primary_user_email);
  policy::DeviceLocalAccountPolicyBroker* broker =
      connector->GetDeviceLocalAccountPolicyService()->GetBrokerForUser(
          primary_user_email.value());
  if (broker) {
    auto id = installers_.Add(std::move(installer));
    installers_.Get(id)->SetForceInstallExtensionsFromCache(
        broker->GetCachedExtensions());
  } else {
    LOG(ERROR) << "Missing broker for DeviceLocalAccount";
    crash_reporter::DumpWithoutCrashing();
  }
}

void DeviceLocalAccountExtensionServiceAsh::SetForceInstallExtensionsFromCache(
    const std::string& device_local_account_user_email,
    const base::Value::Dict& dict) {
  if (device_local_account_user_email != GetPrimaryUserEmail())
    return;
  for (auto& installer : installers_) {
    installer->SetForceInstallExtensionsFromCache(dict.Clone());
  }
}

}  // namespace crosapi

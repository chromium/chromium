// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_local_account_extension_service_ash.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/values.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/crosapi/mojom/device_local_account_extension_service.mojom.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {
namespace {

std::optional<std::string> GetPrimaryUserEmail() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (user) {
    return user->GetAccountId().GetUserEmail();
  }

  return std::nullopt;
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
        pending_installer) {
  const auto id = installers_.Add(std::move(pending_installer));

  installers_.Get(id)->SetForceInstallExtensionsFromCache(
      GetForceInstallExtensionsForPrimaryUser());
}

void DeviceLocalAccountExtensionServiceAsh::SetForceInstallExtensionsFromCache(
    const std::string& device_local_account_user_email,
    const base::Value::Dict& dict) {
  user_email_to_extensions_dict_[device_local_account_user_email] =
      dict.Clone();

  if (device_local_account_user_email != GetPrimaryUserEmail()) {
    return;
  }
  for (auto& installer : installers_) {
    installer->SetForceInstallExtensionsFromCache(dict.Clone());
  }
}

base::Value::Dict
DeviceLocalAccountExtensionServiceAsh::GetForceInstallExtensionsForPrimaryUser()
    const {
  std::optional<std::string> primary_user_email = GetPrimaryUserEmail();
  CHECK(primary_user_email.has_value());
  if (!user_email_to_extensions_dict_.contains(primary_user_email.value())) {
    // The force install extensions list for the primary user is not available
    // yet. Send an empty dict for now and the populated dict will be sent
    // later (through `SetForceInstallExtensionsFromCache`).
    return base::Value::Dict();
  }

  return user_email_to_extensions_dict_.at(primary_user_email.value()).Clone();
}

}  // namespace crosapi

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/app_mode/device_local_account_extension_installer_lacros.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/external_loader/device_local_account_external_policy_loader.h"
#include "chromeos/crosapi/mojom/device_local_account_extension_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace {

DeviceLocalAccountExtensionInstallerLacros* g_instance = nullptr;

}  // namespace

DeviceLocalAccountExtensionInstallerLacros::
    DeviceLocalAccountExtensionInstallerLacros() {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<
          crosapi::mojom::DeviceLocalAccountExtensionService>()) {
    LOG(WARNING) << "DeviceLocalAccountExtensionService is not available";
    return;
  }

  service->GetRemote<crosapi::mojom::DeviceLocalAccountExtensionService>()
      ->BindExtensionInstaller(
          installer_receiver_.BindNewPipeAndPassRemoteWithVersion());
  extension_loader_ =
      base::MakeRefCounted<chromeos::DeviceLocalAccountExternalPolicyLoader>();
  g_instance = this;
}

DeviceLocalAccountExtensionInstallerLacros::
    ~DeviceLocalAccountExtensionInstallerLacros() {
  g_instance = nullptr;
}

DeviceLocalAccountExtensionInstallerLacros*
DeviceLocalAccountExtensionInstallerLacros::Get() {
  return g_instance;
}

void DeviceLocalAccountExtensionInstallerLacros::
    SetForceInstallExtensionsFromCache(base::Value::Dict dict) {
  extension_loader_->OnExtensionListsUpdated(std::move(dict));
}

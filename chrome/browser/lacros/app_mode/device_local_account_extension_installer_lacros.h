// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_APP_MODE_DEVICE_LOCAL_ACCOUNT_EXTENSION_INSTALLER_LACROS_H_
#define CHROME_BROWSER_LACROS_APP_MODE_DEVICE_LOCAL_ACCOUNT_EXTENSION_INSTALLER_LACROS_H_

#include "chrome/browser/chromeos/extensions/external_loader/device_local_account_external_policy_loader.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chromeos/crosapi/mojom/device_local_account_extension_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Lacros implementation of the
// crosapi::mojom::DeviceLocalAccountExtensionInstaller interface.
//
// This class registers itself as a singleton so that the extension system can
// access it and register its loader.
class DeviceLocalAccountExtensionInstallerLacros
    : public crosapi::mojom::DeviceLocalAccountExtensionInstaller {
 public:
  DeviceLocalAccountExtensionInstallerLacros();
  DeviceLocalAccountExtensionInstallerLacros(
      const DeviceLocalAccountExtensionInstallerLacros&) = delete;
  DeviceLocalAccountExtensionInstallerLacros& operator=(
      const DeviceLocalAccountExtensionInstallerLacros&) = delete;
  ~DeviceLocalAccountExtensionInstallerLacros() override;

  // crosapi::mojom::DeviceLocalAccountExtensionInstaller
  void SetForceInstallExtensionsFromCache(base::Value::Dict dict) override;
  scoped_refptr<extensions::ExternalLoader> extension_loader() {
    return extension_loader_;
  }

  static DeviceLocalAccountExtensionInstallerLacros* Get();

 private:
  scoped_refptr<chromeos::DeviceLocalAccountExternalPolicyLoader>
      extension_loader_;
  mojo::Receiver<crosapi::mojom::DeviceLocalAccountExtensionInstaller>
      installer_receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_APP_MODE_DEVICE_LOCAL_ACCOUNT_EXTENSION_INSTALLER_LACROS_H_

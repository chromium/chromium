// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DEVICE_LOCAL_ACCOUNT_EXTENSION_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DEVICE_LOCAL_ACCOUNT_EXTENSION_SERVICE_ASH_H_

#include <map>
#include <string>

#include "base/values.h"
#include "chromeos/crosapi/mojom/device_local_account_extension_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Implementation of the crosapi::mojom::DeviceLocalAccountExtensionService
// interface.
//
// This class will be informed about the force install extensions for all device
// local accounts, but Lacros only cares about the force install extensions of
// the primary (logged in) user. However this class exists at the login screen
// where the primary user is still unknown, so `extensions_by_user_id_` will
// track all this information and send the right force install extensions
// dictionary when Lacros first comes up. Subsequent updates will be passed
// directly via SetForceInstallExtensionsFromCache.
class DeviceLocalAccountExtensionServiceAsh
    : public crosapi::mojom::DeviceLocalAccountExtensionService {
 public:
  DeviceLocalAccountExtensionServiceAsh();
  DeviceLocalAccountExtensionServiceAsh(
      const DeviceLocalAccountExtensionServiceAsh&) = delete;
  DeviceLocalAccountExtensionServiceAsh& operator=(
      const DeviceLocalAccountExtensionServiceAsh&) = delete;
  ~DeviceLocalAccountExtensionServiceAsh() override;
  // Bind this receiver for `mojom::DeviceLocalAccountExtensionService`. This is
  // used by crosapi.
  void BindReceiver(
      mojo::PendingReceiver<mojom::DeviceLocalAccountExtensionService>
          pending_receiver);

  // mojom::DeviceLocalAccountExtensionService:
  void BindExtensionInstaller(
      mojo::PendingRemote<mojom::DeviceLocalAccountExtensionInstaller>
          installer) override;

  // Forward the install information for force installed extensions to Lacros.
  // This call will check that the |device_local_account_user_email| belongs to
  // the currently logged in user before forwarding the call.
  void SetForceInstallExtensionsFromCache(
      const std::string& device_local_account_user_email,
      const base::Value::Dict& dict);

 private:
  base::Value::Dict GetForceInstallExtensionsForPrimaryUser() const;

  mojo::ReceiverSet<mojom::DeviceLocalAccountExtensionService> receivers_;
  mojo::RemoteSet<mojom::DeviceLocalAccountExtensionInstaller> installers_;

  // Tracks the latest force-install-extensions dictionary sent to
  // `SetForceInstallExtensionsFromCache`, keyed on the corresponding
  // `device_local_account_user_email`.
  std::map<std::string, base::Value::Dict> user_email_to_extensions_dict_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DEVICE_LOCAL_ACCOUNT_EXTENSION_SERVICE_ASH_H_

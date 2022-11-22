// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_SMB_FS_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_SMB_FS_SERVICE_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// SmbFsServiceProvider exposes a D-Bus method which is used by instances of
// SmbFs to bootstrap a Mojo IPC connection. The method by which SmbFs is
// started cannot be passed a file descriptor, therefore this D-Bus method is
// used to asynchronously associate a FD with an identified SmbFs instance.
class SmbFsServiceProvider : public CrosDBusService::ServiceProviderInterface {
 public:
  SmbFsServiceProvider();

  SmbFsServiceProvider(const SmbFsServiceProvider&) = delete;
  SmbFsServiceProvider& operator=(const SmbFsServiceProvider&) = delete;

  ~SmbFsServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Handler for the OpenIpcChannel() D-Bus method.
  void HandleOpenIpcChannel(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  base::WeakPtrFactory<SmbFsServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_SMB_FS_SERVICE_PROVIDER_H_

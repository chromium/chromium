// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_VM_VM_MANAGEMENT_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_VM_VM_MANAGEMENT_SERVICE_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// This class exports D-Bus methods for managing VMs.
class VmManagementServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  VmManagementServiceProvider();
  VmManagementServiceProvider(const VmManagementServiceProvider&) = delete;
  VmManagementServiceProvider& operator=(const VmManagementServiceProvider&) =
      delete;
  ~VmManagementServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  void SetCrostiniVmType(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender);

  base::WeakPtrFactory<VmManagementServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_VM_VM_MANAGEMENT_SERVICE_PROVIDER_H_

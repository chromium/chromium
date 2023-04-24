// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_VM_VM_LAUNCH_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_VM_VM_LAUNCH_SERVICE_PROVIDER_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace ash {

class VmLaunchServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  VmLaunchServiceProvider();
  ~VmLaunchServiceProvider() override;

  // Delete copy constructor/assign.
  VmLaunchServiceProvider(const VmLaunchServiceProvider&) = delete;
  VmLaunchServiceProvider& operator=(const VmLaunchServiceProvider&) = delete;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Sets the token used by certain VMs to authorize their usage. This method
  // expects a string for the token, and optionally a boolean to control whether
  // we should install/launch on successfully setting the token, or just return.
  //
  // Example usage:
  // dbus-send --print-reply --system --type=method_call                 \
  //   --dest=org.chromium.VmLaunchService /org/chromium/VmLaunchService \
  //   org.chromium.VmLaunchService.ProvideVmToken                       \
  //   string:<TOKEN> boolean:<bool>
  //
  // TODO(b/218403711): This API is temporary, and will be removed. Criteria for
  // its removal is documented in the linked bug.
  void ProvideVmToken(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender);

  void EnsureVmLaunched(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender);

  base::WeakPtrFactory<VmLaunchServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_VM_VM_LAUNCH_SERVICE_PROVIDER_H_

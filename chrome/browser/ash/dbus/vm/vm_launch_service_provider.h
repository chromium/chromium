// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_VM_VM_LAUNCH_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_VM_VM_LAUNCH_SERVICE_PROVIDER_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace borealis {
class BorealisCapabilities;
}

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
  void StartWaylandServer(dbus::MethodCall* method_call,
                          dbus::ExportedObject::ResponseSender response_sender);

  void OnWaylandServerStarted(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender,
      borealis::BorealisCapabilities* capabilities,
      const base::FilePath& path);

  void StopWaylandServer(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender);

  base::WeakPtrFactory<VmLaunchServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_VM_VM_LAUNCH_SERVICE_PROVIDER_H_

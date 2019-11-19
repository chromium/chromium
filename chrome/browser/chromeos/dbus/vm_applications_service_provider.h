// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_VM_APPLICATIONS_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_VM_APPLICATIONS_SERVICE_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace chromeos {

// This class exports D-Bus methods for functions that we want to be available
// to the Crostini container.
class VmApplicationsServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  VmApplicationsServiceProvider();
  ~VmApplicationsServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when UpdateApplicationList() is exported as a
  // D-Bus method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to a D-Bus request.
  void UpdateApplicationList(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);
  void LaunchTerminal(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender);
  void UpdateMimeTypes(dbus::MethodCall* method_call,
                       dbus::ExportedObject::ResponseSender response_sender);

  base::WeakPtrFactory<VmApplicationsServiceProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VmApplicationsServiceProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_VM_APPLICATIONS_SERVICE_PROVIDER_H_

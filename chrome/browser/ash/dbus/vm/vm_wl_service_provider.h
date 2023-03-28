// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_VM_VM_WL_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_VM_VM_WL_SERVICE_PROVIDER_H_

#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"

#include "dbus/exported_object.h"

namespace ash {

class VmWlServiceProvider : public CrosDBusService::ServiceProviderInterface {
 public:
  VmWlServiceProvider();
  ~VmWlServiceProvider() override;

  // Delete copy constructor/assign.
  VmWlServiceProvider(const VmWlServiceProvider&) = delete;
  VmWlServiceProvider& operator=(const VmWlServiceProvider&) = delete;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  void ListenOnSocket(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender);

  void CloseSocket(dbus::MethodCall* method_call,
                   dbus::ExportedObject::ResponseSender response_sender);

  base::WeakPtrFactory<VmWlServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_VM_VM_WL_SERVICE_PROVIDER_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_VM_VM_SK_FORWARDING_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_VM_VM_SK_FORWARDING_SERVICE_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// This class exports D-Bus methods for functions used by Crostini VMs for
// Security Key forwarding.
class VmSKForwardingServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  VmSKForwardingServiceProvider();
  VmSKForwardingServiceProvider(const VmSKForwardingServiceProvider&) = delete;
  VmSKForwardingServiceProvider& operator=(
      const VmSKForwardingServiceProvider&) = delete;
  ~VmSKForwardingServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when ForwardSecurityKeyMessage is exported as a
  // D-Bus method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to a D-Bus request.
  // Forwards message to the extension and returns response from extensions,
  // if any.
  void ForwardSecurityKeyMessage(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Callback executed after a response to a Security Key request is available.
  // Passes the response to the client over D-Bus.
  void OnResponse(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender response_sender,
                  const std::string& response_message);

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<VmSKForwardingServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_VM_VM_SK_FORWARDING_SERVICE_PROVIDER_H_

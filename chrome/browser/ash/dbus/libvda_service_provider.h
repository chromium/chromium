// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_LIBVDA_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_LIBVDA_SERVICE_PROVIDER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "mojo/public/cpp/system/handle.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// This class exports a D-Bus method that libvda will call to establish a
// mojo pipe to the VideoAcceleratorFactory interface.
class LibvdaServiceProvider : public CrosDBusService::ServiceProviderInterface {
 public:
  LibvdaServiceProvider();

  LibvdaServiceProvider(const LibvdaServiceProvider&) = delete;
  LibvdaServiceProvider& operator=(const LibvdaServiceProvider&) = delete;

  ~LibvdaServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when a handler is exported as a D-Bus
  // method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to D-Bus requests.
  void ProvideMojoConnection(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void OnBootstrapVideoAcceleratorFactoryCallback(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender,
      mojo::ScopedHandle handle,
      const std::string& s);

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<LibvdaServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_LIBVDA_SERVICE_PROVIDER_H_

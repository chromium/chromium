// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_VIRTUAL_FILE_REQUEST_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_VIRTUAL_FILE_REQUEST_SERVICE_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// VirtualFileRequestServiceProvider exposes D-Bus methods which will be
// called by the VirtualFileProvider service.
class VirtualFileRequestServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  VirtualFileRequestServiceProvider();

  VirtualFileRequestServiceProvider(const VirtualFileRequestServiceProvider&) =
      delete;
  VirtualFileRequestServiceProvider& operator=(
      const VirtualFileRequestServiceProvider&) = delete;

  ~VirtualFileRequestServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called on UI thread to handle incoming D-Bus method calls.
  void HandleReadRequest(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender);
  void HandleIdReleased(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender);

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<VirtualFileRequestServiceProvider> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_VIRTUAL_FILE_REQUEST_SERVICE_PROVIDER_H_

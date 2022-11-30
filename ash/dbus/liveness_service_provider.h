// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DBUS_LIVENESS_SERVICE_PROVIDER_H_
#define ASH_DBUS_LIVENESS_SERVICE_PROVIDER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}

namespace ash {

// This class exports a "CheckLiveness" D-Bus method that the session manager
// calls periodically to confirm that Chrome's UI thread is responsive to D-Bus
// messages.  It can be tested with the following command:
//
// % dbus-send --system --type=method_call --print-reply
//     --dest=org.chromium.LivenessService
//     /org/chromium/LivenessService
//     org.chromium.LivenessServiceInterface.CheckLiveness
//
// -> method return sender=:1.9 -> dest=:1.27 reply_serial=2
//
// (An empty response should be returned.)
class LivenessServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  LivenessServiceProvider();

  LivenessServiceProvider(const LivenessServiceProvider&) = delete;
  LivenessServiceProvider& operator=(const LivenessServiceProvider&) = delete;

  ~LivenessServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when CheckLiveness() is exported as a D-Bus
  // method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to a D-Bus request.
  void CheckLiveness(dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender response_sender);

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<LivenessServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_DBUS_LIVENESS_SERVICE_PROVIDER_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_LOCK_TO_SINGLE_USER_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_LOCK_TO_SINGLE_USER_SERVICE_PROVIDER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/handlers/lock_to_single_user_manager.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// This class exports D-Bus methods for forcing a reboot after sign-out.
//
// VmStarting:
// % dbus-send --system --type=method_call --print-reply
//     --dest=org.chromium.LockToSingleUserService
//     /org/chromium/LockToSingleUserService
//     org.chromium.LockToSingleUserServiceInterface.VmStarting
//
// The method checks if the DeviceRebootOnUserSignout requires a reboot after
// sign-out and uses the TPM to ensure that other users can not log in.

class LockToSingleUserServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  LockToSingleUserServiceProvider();

  LockToSingleUserServiceProvider(const LockToSingleUserServiceProvider&) =
      delete;
  LockToSingleUserServiceProvider& operator=(
      const LockToSingleUserServiceProvider&) = delete;

  ~LockToSingleUserServiceProvider() override;

  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);
  void NotifyVmStarting(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender);
  base::WeakPtrFactory<LockToSingleUserServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_LOCK_TO_SINGLE_USER_SERVICE_PROVIDER_H_

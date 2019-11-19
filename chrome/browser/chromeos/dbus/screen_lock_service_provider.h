// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_SCREEN_LOCK_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_SCREEN_LOCK_SERVICE_PROVIDER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}

namespace chromeos {

// This class exports a D-Bus method that the session_manager calls to instruct
// Chrome to show the lock screen.
class ScreenLockServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  ScreenLockServiceProvider();
  ~ScreenLockServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when a handler is exported as a D-Bus
  // method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to D-Bus requests.
  void ShowLockScreen(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender);

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<ScreenLockServiceProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ScreenLockServiceProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_SCREEN_LOCK_SERVICE_PROVIDER_H_

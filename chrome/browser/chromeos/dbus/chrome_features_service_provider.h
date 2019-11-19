// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_CHROME_FEATURES_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_CHROME_FEATURES_SERVICE_PROVIDER_H_

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

// This class exports D-Bus methods for querying Chrome Features enablement.
//
// IsCrostiniEnabled:
// % dbus-send --system --type=method_call --print-reply
//     --dest=org.chromium.ChromeFeaturesService
//     /org/chromium/ChromeFeaturesService
//     org.chromium.ChromeFeaturesServiceInterface.IsCrostiniEnabled
//     string:"|user id hash|"
//
// % (If |user id hash| is set correctly, returns true if Crostini is enabled
//    for the user identified by the hash, and false otherwise)
//
// IsPluginVmEnabled:
// % dbus-send --system --type=method_call --print-reply
//     --dest=org.chromium.ChromeFeaturesService
//     /org/chromium/ChromeFeaturesService
//     org.chromium.ChromeFeaturesServiceInterface.IsPluginVmEnabled
//     string:"|user id hash|"
//
// % (If |user id hash| is set correctly, returns true if Plugin VMs are enabled
//    for the user identified by the hash, and false otherwise)
//
// Both methods will return an error if the user ID hash parameter is missing.
// Passing an empty string as the user ID hash to either method will
// result in the active user profile being used.

class ChromeFeaturesServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  ChromeFeaturesServiceProvider();
  ~ChromeFeaturesServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when IsCrostiniEnabled() is exported as a D-Bus
  // method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to a D-Bus request.
  void IsFeatureEnabled(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender);
  void IsCrostiniEnabled(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender);
  void IsPluginVmEnabled(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender);
  void IsUsbguardEnabled(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender);
  void IsVmManagementCliAllowed(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<ChromeFeaturesServiceProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeFeaturesServiceProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_CHROME_FEATURES_SERVICE_PROVIDER_H_

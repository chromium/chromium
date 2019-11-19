// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_PLUGIN_VM_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_PLUGIN_VM_SERVICE_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace chromeos {

// This class exports D-Bus methods for querying PluginVm information,
// and to show the PluginVm settings page.
//
// GetLicenseData:
// % dbus-send --system --type=method_call --print-reply
// --dest=org.chromium.PluginVmService /org/chromium/PluginVmService
// org.chromium.PluginVmServiceInterface.GetLicenseData
//
// % (returns message GetLicenseDataResponse {
//  string license_key = 1; // If available, this contains the PluginVm
//                          // license key, if not, this contains the
//                          // empty string.
//  string device_id = 2; // If it is available, this contains the
//                        // directory API ID, if not, this contains
//                        // the empty string.
//
// ShowSettingsPage:
// % dbus-send --system --type=method_call --print-reply
//     --dest=org.chromium.PluginVmService /org/chromium/PluginVmService
//     org.chromium.PluginVmServiceInterface.ShowSettingsPage
//     array:byte:0x0a,0x10,0x70,0x6c,0x75,0x67,0x69,0x6e,0x56,0x6d,0x2f,0x64,
//     0x65,0x74,0x61,0x69,0x6c,0x73
// })
class PluginVmServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  PluginVmServiceProvider();
  ~PluginVmServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when GetLicenseDataResponse() is exported as a
  // D-Bus method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called from PluginVm process in response to a D-Bus request.
  void GetLicenseData(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender);

  // Called from PluginVm process to show the settings page.
  void ShowSettingsPage(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender);

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<PluginVmServiceProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PluginVmServiceProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_PLUGIN_VM_SERVICE_PROVIDER_H_

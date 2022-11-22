// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_KIOSK_INFO_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_KIOSK_INFO_SERVICE_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// Exports a "GetKioskAppRequiredPlatformVersion" D-Bus method that
// update_engine calls to get the required platform version of the
// kiosk app that is configured to auto launch.
// See http://crbug.com/577783 for details.
class KioskInfoService : public CrosDBusService::ServiceProviderInterface {
 public:
  KioskInfoService();

  KioskInfoService(const KioskInfoService&) = delete;
  KioskInfoService& operator=(const KioskInfoService&) = delete;

  ~KioskInfoService() override;

  // CrosDBusService::ServiceProviderInterface
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when CheckLiveness() is exported as a D-Bus
  // method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to a D-Bus request.
  void GetKioskAppRequiredPlatformVersion(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  base::WeakPtrFactory<KioskInfoService> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_KIOSK_INFO_SERVICE_PROVIDER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_ARC_CROSH_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_ARC_CROSH_SERVICE_PROVIDER_H_

#include <memory>
#include <string>

#include "ash/components/arc/mojom/crosh.mojom-forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"

namespace ash {

// This class exports a D-Bus method for accessing ARC resources. It translates
// and proxies D-Bus requests to Mojo message toward ARC.
class ArcCroshServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  ArcCroshServiceProvider();
  ArcCroshServiceProvider(const ArcCroshServiceProvider&) = delete;
  ArcCroshServiceProvider& operator=(const ArcCroshServiceProvider&) = delete;
  ~ArcCroshServiceProvider() override;

  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  void Request(dbus::MethodCall* method_call,
               dbus::ExportedObject::ResponseSender response_sender);

  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Sends D-Bus method response to the caller.
  void SendResponse(std::unique_ptr<dbus::Response> response,
                    dbus::ExportedObject::ResponseSender response_sender,
                    arc::mojom::ArcShellExecutionResultPtr result_ptr);

  base::WeakPtrFactory<ArcCroshServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_ARC_CROSH_SERVICE_PROVIDER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DBUS_GESTURE_PROPERTIES_SERVICE_PROVIDER_H_
#define ASH_DBUS_GESTURE_PROPERTIES_SERVICE_PROVIDER_H_

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/ozone/public/mojom/gesture_properties_service.mojom.h"

namespace dbus {
class MethodCall;
}

namespace ash {

/**
 * Provides a D-Bus bridge to the Mojo GesturePropertiesService, allowing
 * gesture properties to be easily modified. See the Google-internal design doc
 * at go/cros-gesture-properties-dbus-design for more details.
 */
class ASH_EXPORT GesturePropertiesServiceProvider
    : public chromeos::CrosDBusService::ServiceProviderInterface {
 public:
  GesturePropertiesServiceProvider();
  ~GesturePropertiesServiceProvider() override;

  void set_service_for_test(
      ui::ozone::mojom::GesturePropertiesService* service) {
    service_for_test_ = service;
  }

  // CrosDBusService::ServiceProviderInterface
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when CheckLiveness() is exported as a D-Bus
  // method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to a D-Bus request.
  void ListDevices(dbus::MethodCall* method_call,
                   dbus::ExportedObject::ResponseSender response_sender);

  // Called on UI thread in response to a D-Bus request.
  void ListProperties(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender);

  // Called on UI thread in response to a D-Bus request.
  void GetProperty(dbus::MethodCall* method_call,
                   dbus::ExportedObject::ResponseSender response_sender);

  // Called on UI thread in response to a D-Bus request.
  void SetProperty(dbus::MethodCall* method_call,
                   dbus::ExportedObject::ResponseSender response_sender);

  ui::ozone::mojom::GesturePropertiesService* GetService();

  mojo::Remote<ui::ozone::mojom::GesturePropertiesService> service_;
  ui::ozone::mojom::GesturePropertiesService* service_for_test_ = nullptr;

  base::WeakPtrFactory<GesturePropertiesServiceProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GesturePropertiesServiceProvider);
};

}  // namespace ash

#endif  // ASH_DBUS_GESTURE_PROPERTIES_SERVICE_PROVIDER_H_

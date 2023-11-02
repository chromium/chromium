// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DBUS_PRIVACY_SCREEN_SERVICE_PROVIDER_H_
#define ASH_DBUS_PRIVACY_SCREEN_SERVICE_PROVIDER_H_

#include "ash/dbus/privacy_screen/privacy_screen.pb.h"
#include "ash/display/privacy_screen_controller.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}

namespace ash {

// This class exports a D-Bus service that platform daemons / userspace interact
// with to understand the current privacy screen setting. Unit test is in
// ash/display/privacy_screen_controller_unittest.cc file.
class ASH_EXPORT PrivacyScreenServiceProvider
    : public CrosDBusService::ServiceProviderInterface,
      public PrivacyScreenController::Observer {
 public:
  PrivacyScreenServiceProvider();
  PrivacyScreenServiceProvider(const PrivacyScreenServiceProvider&) = delete;
  PrivacyScreenServiceProvider& operator=(const PrivacyScreenServiceProvider&) =
      delete;
  ~PrivacyScreenServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

  // PrivacyScreenController::Observer:
  void OnPrivacyScreenSettingChanged(bool enabled, bool notify_ui) override;

 private:
  // Called from ExportedObject when a handler is exported as a D-Bus method or
  // failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  void GetPrivacyScreenSetting(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // A reference to ExportedObject for sending signals.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  base::ScopedObservation<PrivacyScreenController,
                          PrivacyScreenController::Observer>
      privacy_screen_observation_{this};

  privacy_screen::PrivacyScreenSetting_PrivacyScreenState state_ =
      privacy_screen::PrivacyScreenSetting_PrivacyScreenState_NOT_SUPPORTED;

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<PrivacyScreenServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_DBUS_PRIVACY_SCREEN_SERVICE_PROVIDER_H_

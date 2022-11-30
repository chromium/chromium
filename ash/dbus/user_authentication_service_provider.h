// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DBUS_USER_AUTHENTICATION_SERVICE_PROVIDER_H_
#define ASH_DBUS_USER_AUTHENTICATION_SERVICE_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}

namespace ash {

// This class exports a D-Bus method that platform daemons call to request Ash
// to start in-session user authentication flow.
class UserAuthenticationServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  UserAuthenticationServiceProvider();
  UserAuthenticationServiceProvider(const UserAuthenticationServiceProvider&) =
      delete;
  UserAuthenticationServiceProvider& operator=(
      const UserAuthenticationServiceProvider&) = delete;
  ~UserAuthenticationServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when a handler is exported as a D-Bus
  // method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to D-Bus requests. This expects the
  // request_id field to be a string.
  void ShowAuthDialog(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender);

  // Called when the user authentication flow completes.
  void OnAuthFlowComplete(dbus::MethodCall* method_call,
                          dbus::ExportedObject::ResponseSender response_sender,
                          bool success);

  // Called on UI thread in response to D-Bus requests. Dismisses the dialog if
  // it's shown.
  void Cancel(dbus::MethodCall* method_call,
              dbus::ExportedObject::ResponseSender response_sender);

  // Called on UI thread in response to D-Bus requests.
  void IsAuthenticatorAvailable(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Called when authenticator availability is checked.
  void OnAvailabilityChecked(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender,
      bool available);

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<UserAuthenticationServiceProvider> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_DBUS_USER_AUTHENTICATION_SERVICE_PROVIDER_H_

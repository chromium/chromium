// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_FJORD_OOBE_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_FJORD_OOBE_SERVICE_PROVIDER_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/fjord_oobe/fjord_oobe_state_manager.h"
#include "chrome/browser/ash/login/fjord_oobe/proto/fjord_oobe_state.pb.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}

namespace ash {

// FjordOobeServiceProvider implements the org.chromium.FjordOobeService D-Bus
// interface. It is used by the Fjord variant of OOBE and allows for IPC to
// control parts of OOBE that are specific to Fjord OOBE.
class FjordOobeServiceProvider
    : public CrosDBusService::ServiceProviderInterface,
      FjordOobeStateManager::Observer {
 public:
  FjordOobeServiceProvider();

  FjordOobeServiceProvider(const FjordOobeServiceProvider&) = delete;
  FjordOobeServiceProvider& operator=(const FjordOobeServiceProvider&) = delete;

  ~FjordOobeServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when a handler is exported as a D-Bus
  // method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // FjordOobeStateManager::Observer:
  void OnFjordOobeStateChanged(
      fjord_oobe_state::proto::FjordOobeStateInfo new_state) override;

  void ExitTouchControllerScreen(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);
  void GetOobeState(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);
  void SetOobeState(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  // A reference on ExportedObject for sending signals.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  base::WeakPtrFactory<FjordOobeServiceProvider> weak_ptr_factory_{this};
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_FJORD_OOBE_SERVICE_PROVIDER_H_

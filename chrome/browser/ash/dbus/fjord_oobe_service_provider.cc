// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/fjord_oobe_service_provider.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
FjordOobeServiceProvider::FjordOobeServiceProvider() = default;
FjordOobeServiceProvider::~FjordOobeServiceProvider() = default;

void FjordOobeServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kFjordOobeServiceInterface,
      chromeos::kFjordOobeServiceExitTcSetupMethod,
      base::BindRepeating(&FjordOobeServiceProvider::ExitTouchControllerScreen,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FjordOobeServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FjordOobeServiceProvider::OnExported(const std::string& interface_name,
                                          const std::string& method_name,
                                          bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void FjordOobeServiceProvider::ExitTouchControllerScreen(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  bool is_success = false;
  WizardController* wizard_controller = WizardController::default_controller();
  if (wizard_controller) {
    is_success = wizard_controller->ExitFjordTouchControllerScreen();
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(is_success);

  std::move(response_sender).Run(std::move(response));
}
}  // namespace ash

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/fjord_oobe_service_provider.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/login/fjord_oobe/fjord_oobe_state_manager.h"
#include "chrome/browser/ash/login/fjord_oobe/fjord_oobe_util.h"
#include "chrome/browser/ash/login/fjord_oobe/proto/fjord_oobe_state.pb.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
FjordOobeServiceProvider::FjordOobeServiceProvider() = default;

FjordOobeServiceProvider::~FjordOobeServiceProvider() {
  FjordOobeStateManager::Get()->RemoveObserver(this);
  FjordOobeStateManager::Shutdown();
}

void FjordOobeServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object_ = exported_object;
  exported_object_->ExportMethod(
      chromeos::kFjordOobeServiceInterface,
      chromeos::kFjordOobeServiceExitTcSetupMethod,
      base::BindRepeating(&FjordOobeServiceProvider::ExitTouchControllerScreen,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FjordOobeServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object_->ExportMethod(
      chromeos::kFjordOobeServiceInterface,
      chromeos::kFjordOobeServiceGetFjordOobeStateMethod,
      base::BindRepeating(&FjordOobeServiceProvider::GetOobeState,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FjordOobeServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object_->ExportMethod(
      chromeos::kFjordOobeServiceInterface,
      chromeos::kFjordOobeServiceSetFjordOobeStateMethod,
      base::BindRepeating(&FjordOobeServiceProvider::SetOobeState,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FjordOobeServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  // FjordOobeServiceProvider owns the initialization and shutdown of this
  // service to ensure that the state manager is available when the dbus service
  // starts.
  FjordOobeStateManager::Initialize();
  FjordOobeStateManager* manager = FjordOobeStateManager::Get();
  manager->AddObserver(this);
}

void FjordOobeServiceProvider::OnExported(const std::string& interface_name,
                                          const std::string& method_name,
                                          bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void FjordOobeServiceProvider::OnFjordOobeStateChanged(
    fjord_oobe_state::proto::FjordOobeStateInfo new_state) {
  dbus::Signal signal(chromeos::kFjordOobeServiceInterface,
                      chromeos::kFjordOobeServiceFjordOobeStateChangedSignal);

  dbus::MessageWriter writer(&signal);
  writer.AppendProtoAsArrayOfBytes(new_state);
  exported_object_->SendSignal(&signal);
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

void FjordOobeServiceProvider::GetOobeState(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  FjordOobeStateManager* manager = FjordOobeStateManager::Get();
  DCHECK(manager);

  fjord_oobe_state::proto::FjordOobeStateInfo state =
      manager->GetFjordOobeStateInfo();
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(state);
  std::move(response_sender).Run(std::move(response));
}

void FjordOobeServiceProvider::SetOobeState(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  // Wrap response in a proto because the calling service does not support
  // dbus::Error for now.
  fjord_oobe_state::proto::SetFjordOobeStateResponse response_proto;

  dbus::MessageReader reader(method_call);
  fjord_oobe_state::proto::FjordOobeStateInfo new_state;

  if (!reader.PopArrayOfBytesAsProto(&new_state)) {
    response_proto.set_success(false);
    response_proto.set_error_message(
        "Cannot parse FjordOobeStateInfo proto from the method call");
    writer.AppendProtoAsArrayOfBytes(response_proto);
    std::move(response_sender).Run(std::move(response));
    return;
  }

  WizardController* wizard_controller = WizardController::default_controller();
  if (!wizard_controller) {
    response_proto.set_success(false);
    response_proto.set_error_message(
        "WizardController was not available to change OOBE UI.");
    writer.AppendProtoAsArrayOfBytes(response_proto);
    std::move(response_sender).Run(std::move(response));
    return;
  }

  bool is_success =
      wizard_controller->ShowNextFjordOobeScreen(new_state.oobe_state());
  response_proto.set_success(is_success);
  response_proto.set_error_message(
      is_success ? "" : "OOBE FW update screen could not be exited.");
  writer.AppendProtoAsArrayOfBytes(response_proto);
  std::move(response_sender).Run(std::move(response));
}
}  // namespace ash

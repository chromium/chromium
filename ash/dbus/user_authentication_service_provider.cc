// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/user_authentication_service_provider.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/osauth/impl/request/webauthn_auth_request.h"
#include "chromeos/components/webauthn/webauthn_request_registrar.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

UserAuthenticationServiceProvider::UserAuthenticationServiceProvider() =
    default;

UserAuthenticationServiceProvider::~UserAuthenticationServiceProvider() =
    default;

void UserAuthenticationServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kUserAuthenticationServiceInterface,
      chromeos::kUserAuthenticationServiceShowAuthDialogV2Method,
      base::BindRepeating(&UserAuthenticationServiceProvider::ShowAuthDialog,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&UserAuthenticationServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object->ExportMethod(
      chromeos::kUserAuthenticationServiceInterface,
      chromeos::kUserAuthenticationServiceCancelMethod,
      base::BindRepeating(&UserAuthenticationServiceProvider::Cancel,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&UserAuthenticationServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object->ExportMethod(
      chromeos::kUserAuthenticationServiceInterface,
      chromeos::kUserAuthenticationServiceIsAuthenticatorAvailableMethod,
      base::BindRepeating(
          &UserAuthenticationServiceProvider::IsAuthenticatorAvailable,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&UserAuthenticationServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UserAuthenticationServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void UserAuthenticationServiceProvider::ShowAuthDialog(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  std::string rp_id;
  if (!reader.PopString(&rp_id)) {
    LOG(ERROR) << "Unable to parse origin name";
    OnAuthFlowComplete(method_call, std::move(response_sender), false);
    return;
  }
  // TODO(b/156258540): Show RP id in the dialog prompt.
  int verification_type;
  if (!reader.PopInt32(&verification_type)) {
    LOG(ERROR) << "Unable to parse verification_type";
    OnAuthFlowComplete(method_call, std::move(response_sender), false);
    return;
  }
  std::string request_id;
  if (!reader.PopString(&request_id)) {
    LOG(ERROR) << "Unable to parse request id";
    OnAuthFlowComplete(method_call, std::move(response_sender), false);
    return;
  }

  aura::Window* source_window =
      chromeos::webauthn::WebAuthnRequestRegistrar::Get()
          ->GetWindowForRequestId(request_id);
  if (!source_window) {
    LOG(ERROR) << "Cannot find window with the given request id";
    OnAuthFlowComplete(method_call, std::move(response_sender), false);
    return;
  }

  if (ash::features::IsWebAuthNAuthDialogMergeEnabled()) {
    auto* active_session_auth_controller = ActiveSessionAuthController::Get();
    auto webauthn_auth_request = std::make_unique<WebAuthNAuthRequest>(
        rp_id,
        base::BindOnce(&UserAuthenticationServiceProvider::OnAuthFlowComplete,
                       weak_ptr_factory_.GetWeakPtr(), method_call,
                       std::move(response_sender)));
    active_session_auth_controller->ShowAuthDialog(
        std::move(webauthn_auth_request));
    return;
  }

  auto* webauthn_dialog_controller = WebAuthNDialogController::Get();
  webauthn_dialog_controller->ShowAuthenticationDialog(
      source_window, rp_id,
      base::BindOnce(&UserAuthenticationServiceProvider::OnAuthFlowComplete,
                     weak_ptr_factory_.GetWeakPtr(), method_call,
                     std::move(response_sender)));
}

void UserAuthenticationServiceProvider::OnAuthFlowComplete(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    bool success) {
  DCHECK(method_call && !response_sender.is_null());

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(success);
  std::move(response_sender).Run(std::move(response));
}

void UserAuthenticationServiceProvider::Cancel(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  WebAuthNDialogController::Get()->Cancel();
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  std::move(response_sender).Run(std::move(response));
}

void UserAuthenticationServiceProvider::IsAuthenticatorAvailable(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  auto* webauthn_dialog_controller = WebAuthNDialogController::Get();
  webauthn_dialog_controller->CheckAvailability(base::BindOnce(
      &UserAuthenticationServiceProvider::OnAvailabilityChecked,
      weak_ptr_factory_.GetWeakPtr(), method_call, std::move(response_sender)));
}

void UserAuthenticationServiceProvider::OnAvailabilityChecked(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    bool available) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(available);
  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash

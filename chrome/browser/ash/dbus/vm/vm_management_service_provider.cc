// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/vm/vm_management_service_provider.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/dbus/service_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

void SendResponse(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender response_sender,
                  bool answer) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(answer);
  std::move(response_sender).Run(std::move(response));
}

void OnExported(const std::string& interface_name,
                const std::string& method_name,
                bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

}  // namespace

VmManagementServiceProvider::VmManagementServiceProvider() = default;

VmManagementServiceProvider::~VmManagementServiceProvider() = default;

void VmManagementServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kVmManagementServiceInterface,
      chromeos::kVmManagementServiceSetCrostiniVmTypeMethod,
      base::BindRepeating(&VmManagementServiceProvider::SetCrostiniVmType,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&OnExported));
}

void VmManagementServiceProvider::SetCrostiniVmType(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  std::string user_id_hash;

  if (!reader.PopString(&user_id_hash)) {
    LOG(ERROR) << "Failed to pop user_id_hash from incoming message.";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call,
                                                 DBUS_ERROR_INVALID_ARGS,
                                                 "No user_id_hash string arg"));
    return;
  }

  Profile* profile = GetProfileFromUserIdHash(user_id_hash);

  if (!profile) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_FAILED,
            "No profile or user could be found"));
    return;
  }

  std::string reason;
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile, &reason)) {
    LOG(ERROR) << "Crostini is not allowed: " << reason;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Crostini is not allowed"));
    return;
  }

  int vm_type;
  if (!reader.PopInt32(&vm_type)) {
    LOG(ERROR) << "Failed to pop vm_type from incoming message.";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Missing or invalid vm_type int32 arg."));
    return;
  }
  if (vm_type != guest_os::VmType::TERMINA &&
      vm_type != guest_os::VmType::BAGUETTE) {
    LOG(ERROR) << "Invalid vm_type in incoming message: " << vm_type;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call,
                                                 DBUS_ERROR_INVALID_ARGS,
                                                 "Invalid vm_type int32 arg."));
    return;
  }

  bool set_vm_type = guest_os::UpdateContainerVmType(
      profile, vm_type, crostini::kCrostiniDefaultVmName);

  SendResponse(method_call, std::move(response_sender), set_vm_type);
}

}  // namespace ash

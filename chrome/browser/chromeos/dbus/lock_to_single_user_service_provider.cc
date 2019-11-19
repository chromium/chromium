
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/lock_to_single_user_service_provider.h"

#include "base/bind.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

LockToSingleUserServiceProvider::LockToSingleUserServiceProvider() {}
LockToSingleUserServiceProvider::~LockToSingleUserServiceProvider() {}

void LockToSingleUserServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      lock_to_single_user::kLockToSingleUserInterface,
      lock_to_single_user::kNotifyVmStartingMethod,
      base::BindRepeating(&LockToSingleUserServiceProvider::NotifyVmStarting,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&LockToSingleUserServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
}

void LockToSingleUserServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void LockToSingleUserServiceProvider::NotifyVmStarting(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  policy::LockToSingleUserManager* manager =
      policy::LockToSingleUserManager::GetLockToSingleUserManagerInstance();

  if (manager == nullptr) {
    LOG(ERROR) << "VmStarting received before LockToSingleUserManager ready";

    response_sender.Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_FAILED,
        "VmStarting received before LockToSingleUserManager ready"));

    return;
  }

  manager->DbusNotifyVmStarting();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace chromeos

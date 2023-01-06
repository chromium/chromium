// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/liveness_service_provider.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

LivenessServiceProvider::LivenessServiceProvider() {}

LivenessServiceProvider::~LivenessServiceProvider() = default;

void LivenessServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kLivenessServiceInterface,
      chromeos::kLivenessServiceCheckLivenessMethod,
      base::BindRepeating(&LivenessServiceProvider::CheckLiveness,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&LivenessServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LivenessServiceProvider::OnExported(const std::string& interface_name,
                                         const std::string& method_name,
                                         bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void LivenessServiceProvider::CheckLiveness(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace ash

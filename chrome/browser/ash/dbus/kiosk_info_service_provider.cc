// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/kiosk_info_service_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

KioskInfoService::KioskInfoService() {}

KioskInfoService::~KioskInfoService() = default;

void KioskInfoService::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kKioskAppServiceInterface,
      chromeos::kKioskAppServiceGetRequiredPlatformVersionMethod,
      base::BindRepeating(&KioskInfoService::GetKioskAppRequiredPlatformVersion,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&KioskInfoService::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskInfoService::OnExported(const std::string& interface_name,
                                  const std::string& method_name,
                                  bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

void KioskInfoService::GetKioskAppRequiredPlatformVersion(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendString(
      KioskChromeAppManager::Get()->GetAutoLaunchAppRequiredPlatformVersion());
  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash

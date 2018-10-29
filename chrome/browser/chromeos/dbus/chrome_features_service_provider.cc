// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/chrome_features_service_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

ChromeFeaturesServiceProvider::ChromeFeaturesServiceProvider()
    : weak_ptr_factory_(this) {}

ChromeFeaturesServiceProvider::~ChromeFeaturesServiceProvider() = default;

void ChromeFeaturesServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsCrostiniEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsCrostiniEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsUsbguardEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsUsbguardEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsShillSandboxingEnabledMethod,
      base::BindRepeating(
          &ChromeFeaturesServiceProvider::IsShillSandboxingEnabled,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ChromeFeaturesServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void ChromeFeaturesServiceProvider::IsCrostiniEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  std::string user_id_hash;

  if (!reader.PopString(&user_id_hash)) {
    LOG(ERROR) << "Failed to pop user_id_hash from incoming message.";
    response_sender.Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_INVALID_ARGS, "No user_id_hash string arg"));
    return;
  }

  Profile* profile =
      user_id_hash.empty()
          ? ProfileManager::GetActiveUserProfile()
          : g_browser_process->profile_manager()->GetProfileByPath(
                ProfileHelper::GetProfilePathByUserIdHash(user_id_hash));

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(crostini::IsCrostiniAllowedForProfile(profile));
  response_sender.Run(std::move(response));
}

void ChromeFeaturesServiceProvider::IsUsbguardEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(base::FeatureList::IsEnabled(features::kUsbguard));
  response_sender.Run(std::move(response));
}

void ChromeFeaturesServiceProvider::IsShillSandboxingEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(base::FeatureList::IsEnabled(features::kShillSandboxing));
  response_sender.Run(std::move(response));
}

}  // namespace chromeos

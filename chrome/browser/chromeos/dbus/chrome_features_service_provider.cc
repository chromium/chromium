// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/chrome_features_service_provider.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/arc/arc_features.h"
#include "components/prefs/pref_service.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

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

Profile* GetSenderProfile(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender* response_sender) {
  dbus::MessageReader reader(method_call);
  std::string user_id_hash;

  if (!reader.PopString(&user_id_hash)) {
    LOG(ERROR) << "Failed to pop user_id_hash from incoming message.";
    std::move(*response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call,
                                                 DBUS_ERROR_INVALID_ARGS,
                                                 "No user_id_hash string arg"));
    return nullptr;
  }

  if (user_id_hash.empty())
    return ProfileManager::GetActiveUserProfile();

  return g_browser_process->profile_manager()->GetProfileByPath(
      chromeos::ProfileHelper::GetProfilePathByUserIdHash(user_id_hash));
}
}  // namespace

namespace chromeos {

ChromeFeaturesServiceProvider::ChromeFeaturesServiceProvider() {}

ChromeFeaturesServiceProvider::~ChromeFeaturesServiceProvider() = default;

void ChromeFeaturesServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsFeatureEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsFeatureEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ChromeFeaturesServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsCrostiniEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsCrostiniEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ChromeFeaturesServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsPluginVmEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsPluginVmEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ChromeFeaturesServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsUsbguardEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsUsbguardEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ChromeFeaturesServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsCryptohomeDistributedModelEnabledMethod,
      base::BindRepeating(
          &ChromeFeaturesServiceProvider::IsCryptohomeDistributedModelEnabled,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsCryptohomeUserDataAuthEnabledMethod,
      base::BindRepeating(
          &ChromeFeaturesServiceProvider::IsCryptohomeUserDataAuthEnabled,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsCryptohomeUserDataAuthKillswitchEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::
                              IsCryptohomeUserDataAuthKillswitchEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsVmManagementCliAllowedMethod,
      base::BindRepeating(
          &ChromeFeaturesServiceProvider::IsVmManagementCliAllowed,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ChromeFeaturesServiceProvider::OnExported,
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

void ChromeFeaturesServiceProvider::IsFeatureEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  static const base::Feature constexpr* kFeatureLookup[] = {
      &::features::kUsbbouncer,
      &::features::kUsbguard,
      &arc::kBootCompletedBroadcastFeature,
      &arc::kCustomTabsExperimentFeature,
      &arc::kFilePickerExperimentFeature,
      &arc::kNativeBridgeToggleFeature,
      &features::kSessionManagerLongKillTimeout,
  };

  dbus::MessageReader reader(method_call);
  std::string feature_name;
  if (!reader.PopString(&feature_name)) {
    LOG(ERROR) << "Failed to pop feature_name from incoming message.";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Missing or invalid feature_name string arg."));
    return;
  }

  auto* const* it =
      std::find_if(std::begin(kFeatureLookup), std::end(kFeatureLookup),
                   [&feature_name](const base::Feature* feature) -> bool {
                     return feature_name == feature->name;
                   });
  if (it == std::end(kFeatureLookup)) {
    LOG(ERROR) << "Unexpected feature name.";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Unexpected feature name."));
    return;
  }

  SendResponse(method_call, std::move(response_sender),
               base::FeatureList::IsEnabled(**it));
}

void ChromeFeaturesServiceProvider::IsCrostiniEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  Profile* profile = GetSenderProfile(method_call, &response_sender);
  if (!profile)
    return;

  SendResponse(
      method_call, std::move(response_sender),
      profile ? crostini::CrostiniFeatures::Get()->IsAllowed(profile) : false);
}

void ChromeFeaturesServiceProvider::IsCryptohomeDistributedModelEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  SendResponse(
      method_call, std::move(response_sender),
      base::FeatureList::IsEnabled(::features::kCryptohomeDistributedModel));
}

void ChromeFeaturesServiceProvider::IsCryptohomeUserDataAuthEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  SendResponse(
      method_call, std::move(response_sender),
      base::FeatureList::IsEnabled(::features::kCryptohomeUserDataAuth));
}

void ChromeFeaturesServiceProvider::IsCryptohomeUserDataAuthKillswitchEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  SendResponse(method_call, std::move(response_sender),
               base::FeatureList::IsEnabled(
                   ::features::kCryptohomeUserDataAuthKillswitch));
}

void ChromeFeaturesServiceProvider::IsPluginVmEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  Profile* profile = GetSenderProfile(method_call, &response_sender);
  if (!profile)
    return;

  SendResponse(
      method_call, std::move(response_sender),
      profile ? plugin_vm::PluginVmFeatures::Get()->IsAllowed(profile) : false);
}

void ChromeFeaturesServiceProvider::IsUsbguardEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  SendResponse(method_call, std::move(response_sender),
               base::FeatureList::IsEnabled(::features::kUsbguard));
}

void ChromeFeaturesServiceProvider::IsVmManagementCliAllowed(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  Profile* profile = GetSenderProfile(method_call, &response_sender);
  if (!profile)
    return;

  SendResponse(method_call, std::move(response_sender),
               profile->GetPrefs()->GetBoolean(
                   crostini::prefs::kVmManagementCliAllowedByPolicy));
}

}  // namespace chromeos

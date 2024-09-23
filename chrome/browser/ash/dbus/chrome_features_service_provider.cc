// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/chrome_features_service_provider.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// A prefix to apply to all features which Chrome OS platform-side code wishes
// to check.
// This prefix must *only* be applied to features on the platform side, and no
// |base::Feature|s should be defined with this prefix.
// A presubmit will enforce that no |base::Feature|s will be defined with this
// prefix.
// TODO(crbug.com/40202807): Add the aforementioned presubmit.
constexpr char kCrOSLateBootFeaturePrefix[] = "CrOSLateBoot";

void SendResponse(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender response_sender,
                  bool answer,
                  const std::string& reason = std::string()) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(answer);
  if (!reason.empty())
    writer.AppendString(reason);
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
      ProfileHelper::GetProfilePathByUserIdHash(user_id_hash));
}

}  // namespace

ChromeFeaturesServiceProvider::ChromeFeaturesServiceProvider(
    std::unique_ptr<base::FeatureList::Accessor> feature_list_accessor)
    : feature_list_accessor_(std::move(feature_list_accessor)) {}

ChromeFeaturesServiceProvider::~ChromeFeaturesServiceProvider() = default;

void ChromeFeaturesServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kChromeFeaturesServiceInterface,
      chromeos::kChromeFeaturesServiceIsFeatureEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsFeatureEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ChromeFeaturesServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kChromeFeaturesServiceInterface,
      chromeos::kChromeFeaturesServiceGetFeatureParamsMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::GetFeatureParams,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ChromeFeaturesServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kChromeFeaturesServiceInterface,
      chromeos::kChromeFeaturesServiceIsCrostiniEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsCrostiniEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ChromeFeaturesServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kChromeFeaturesServiceInterface,
      chromeos::kChromeFeaturesServiceIsPluginVmEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsPluginVmEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ChromeFeaturesServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kChromeFeaturesServiceInterface,
      chromeos::kChromeFeaturesServiceIsCryptohomeDistributedModelEnabledMethod,
      base::BindRepeating(
          &ChromeFeaturesServiceProvider::IsCryptohomeDistributedModelEnabled,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kChromeFeaturesServiceInterface,
      chromeos::kChromeFeaturesServiceIsCryptohomeUserDataAuthEnabledMethod,
      base::BindRepeating(
          &ChromeFeaturesServiceProvider::IsCryptohomeUserDataAuthEnabled,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kChromeFeaturesServiceInterface,
      chromeos::
          kChromeFeaturesServiceIsCryptohomeUserDataAuthKillswitchEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::
                              IsCryptohomeUserDataAuthKillswitchEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kChromeFeaturesServiceInterface,
      chromeos::kChromeFeaturesServiceIsVmManagementCliAllowedMethod,
      base::BindRepeating(
          &ChromeFeaturesServiceProvider::IsVmManagementCliAllowed,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ChromeFeaturesServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kChromeFeaturesServiceInterface,
      chromeos::kChromeFeaturesServiceIsPeripheralDataAccessEnabledMethod,
      base::BindRepeating(
          &ChromeFeaturesServiceProvider::IsPeripheralDataAccessEnabled,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kChromeFeaturesServiceInterface,
      chromeos::kChromeFeaturesServiceIsDNSProxyEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsDnsProxyEnabled,
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

void ChromeFeaturesServiceProvider::IsFeatureEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  static const base::Feature constexpr* kFeatureLookup[] = {
      &arc::kBootCompletedBroadcastFeature,
      &arc::kCustomTabsExperimentFeature,
      &arc::kFilePickerExperimentFeature,
      &arc::kNativeBridgeToggleFeature,
      &features::kSessionManagerLongKillTimeout,
      &features::kSessionManagerLivenessCheck,
      &features::kBorealisProvision,
      &features::kDeferConciergeStartup,
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
      base::ranges::find(kFeatureLookup, feature_name, &base::Feature::name);
  if (it != std::end(kFeatureLookup)) {
    SendResponse(method_call, std::move(response_sender),
                 base::FeatureList::IsEnabled(**it));
    return;
  }
  // Not on our list. Potentially look up by name instead.
  // Only search for arbitrary trial names that begin with the appropriate
  // prefix, since looking up a feature by name will not be able to get the
  // default value associated with any `base::Feature` defined in the code
  // base.
  // Separately, a presubmit will enforce that no `base::Feature` definition
  // has a name starting with this prefix.
  // TODO(crbug.com/40202807): Add the aforementioned presubmit.
  base::FeatureList::OverrideState state =
      base::FeatureList::OVERRIDE_USE_DEFAULT;
  if (feature_name.find(kCrOSLateBootFeaturePrefix) == 0) {
    state = feature_list_accessor_->GetOverrideStateByFeatureName(feature_name);
  } else {
    LOG(ERROR) << "Invalid prefix on feature " << feature_name << " (want "
               << kCrOSLateBootFeaturePrefix << ")";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            base::StrCat({"Invalid prefix for feature name: '", feature_name,
                          "'. Want ", kCrOSLateBootFeaturePrefix})));
    return;
  }
  if (state == base::FeatureList::OVERRIDE_USE_DEFAULT) {
    VLOG(1) << "Unexpected feature name '" << feature_name << "'"
            << " (likely just indicates there isn't a variations seed).";
    // This isn't really an error, we're just using the error channel to signal
    // to feature_library that it should fall back to its defaults.
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            base::StrCat({"Chrome can't get state for '", feature_name,
                          "'; feature_library will decide"})));
    return;
  }
  SendResponse(method_call, std::move(response_sender),
               state == base::FeatureList::OVERRIDE_ENABLE_FEATURE);
}

void ChromeFeaturesServiceProvider::GetFeatureParams(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  dbus::MessageReader array_reader(nullptr);
  if (!reader.PopArray(&array_reader)) {
    LOG(ERROR) << "Failed to read array of feature names.";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Could not pop string array of feature names"));
    return;
  }

  std::vector<std::string> features;
  std::map<std::string, std::map<std::string, std::string>> params_map;
  std::map<std::string, bool> enabled_map;
  while (array_reader.HasMoreData()) {
    std::string feature_name;

    if (!array_reader.PopString(&feature_name)) {
      LOG(ERROR) << "Failed to pop feature_name from array.";
      std::move(response_sender)
          .Run(dbus::ErrorResponse::FromMethodCall(
              method_call, DBUS_ERROR_INVALID_ARGS,
              "Missing or invalid feature_name string arg in array."));
      return;
    }

    if (feature_name.find(kCrOSLateBootFeaturePrefix) != 0) {
      LOG(ERROR) << "Unexpected prefix on feature name '" << feature_name << "'"
                 << " (want " << kCrOSLateBootFeaturePrefix << ")";
      std::move(response_sender)
          .Run(dbus::ErrorResponse::FromMethodCall(
              method_call, DBUS_ERROR_INVALID_ARGS,
              base::StrCat({"Invalid prefix for feature name: '", feature_name,
                            "'. Want ", kCrOSLateBootFeaturePrefix})));
      return;
    }

    features.push_back(feature_name);

    base::FeatureList::OverrideState state =
        feature_list_accessor_->GetOverrideStateByFeatureName(feature_name);
    if (state == base::FeatureList::OVERRIDE_ENABLE_FEATURE) {
      enabled_map[feature_name] = true;
    } else if (state == base::FeatureList::OVERRIDE_DISABLE_FEATURE) {
      enabled_map[feature_name] = false;
    }
    // else leave it out of the map.

    std::map<std::string, std::string> per_feature_map;
    if (!feature_list_accessor_->GetParamsByFeatureName(feature_name,
                                                        &per_feature_map)) {
      VLOG(1) << "No trial found for '" << feature_name << "', skipping."
              << " (likely just means there is no variations seed)";
      continue;
    }
    params_map[feature_name] = std::move(per_feature_map);
  }

  // Build response
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(nullptr);
  // A map from feature name to:
  // * two booleans:
  //   * Whether to use the override (or the default),
  //   * What the override state is (only valid if we should use the
  //     override value).
  // * Another map, from parameter name to value.
  writer.OpenArray("{s(bba{ss})}", &array_writer);
  for (const auto& feature_name : features) {
    dbus::MessageWriter feature_dict_writer(nullptr);
    array_writer.OpenDictEntry(&feature_dict_writer);
    feature_dict_writer.AppendString(feature_name);
    dbus::MessageWriter struct_writer(nullptr);
    feature_dict_writer.OpenStruct(&struct_writer);

    if (enabled_map.find(feature_name) != enabled_map.end()) {
      struct_writer.AppendBool(true);  // Use override
      struct_writer.AppendBool(enabled_map[feature_name]);
    } else {
      struct_writer.AppendBool(false);  // Ignore override
      struct_writer.AppendBool(false);  // Arbitrary choice
    }

    dbus::MessageWriter sub_array_writer(nullptr);
    struct_writer.OpenArray("{ss}", &sub_array_writer);
    if (params_map.find(feature_name) != params_map.end()) {
      const auto& submap = params_map[feature_name];
      for (const auto& [key, value] : submap) {
        dbus::MessageWriter dict_writer(nullptr);
        sub_array_writer.OpenDictEntry(&dict_writer);
        dict_writer.AppendString(key);
        dict_writer.AppendString(value);
        sub_array_writer.CloseContainer(&dict_writer);
      }
    }
    struct_writer.CloseContainer(&sub_array_writer);
    feature_dict_writer.CloseContainer(&struct_writer);
    array_writer.CloseContainer(&feature_dict_writer);
  }
  writer.CloseContainer(&array_writer);
  std::move(response_sender).Run(std::move(response));
}

void ChromeFeaturesServiceProvider::IsCrostiniEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  Profile* profile = GetSenderProfile(method_call, &response_sender);
  if (!profile)
    return;

  std::string reason;
  bool answer =
      crostini::CrostiniFeatures::Get()->IsAllowedNow(profile, &reason);
  SendResponse(method_call, std::move(response_sender), answer, reason);
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

  std::string reason;
  bool answer = plugin_vm::PluginVmFeatures::Get()->IsAllowed(profile, &reason);
  SendResponse(method_call, std::move(response_sender), answer, reason);
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

void ChromeFeaturesServiceProvider::IsPeripheralDataAccessEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  // TODO(1242686): Add end-to-end tests for this D-Bus signal.

  bool peripheral_data_access_enabled = false;
  // Enterprise managed devices use the local state pref.
  if (InstallAttributes::Get()->IsEnterpriseManaged()) {
    peripheral_data_access_enabled =
        g_browser_process->local_state()->GetBoolean(
            prefs::kLocalStateDevicePeripheralDataAccessEnabled);
  } else {
    // Consumer devices use the CrosSetting pref.
    CrosSettings::Get()->GetBoolean(kDevicePeripheralDataAccessEnabled,
                                    &peripheral_data_access_enabled);
  }
  SendResponse(method_call, std::move(response_sender),
               peripheral_data_access_enabled);
}

void ChromeFeaturesServiceProvider::IsDnsProxyEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  SendResponse(method_call, std::move(response_sender),
               !base::FeatureList::IsEnabled(features::kDisableDnsProxy));
}

}  // namespace ash

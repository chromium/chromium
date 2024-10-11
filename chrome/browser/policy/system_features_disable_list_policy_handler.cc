// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "system_features_disable_list_policy_handler.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace policy {

const char kCameraFeature[] = "camera";
const char kBrowserSettingsFeature[] = "browser_settings";
const char kOsSettingsFeature[] = "os_settings";
const char kScanningFeature[] = "scanning";
const char kWebStoreFeature[] = "web_store";
const char kCanvasFeature[] = "canvas";
const char kExploreFeature[] = "explore";
const char kCroshFeature[] = "crosh";
const char kTerminalFeature[] = "terminal";
const char kGalleryFeature[] = "gallery";
const char kPrintJobsFeature[] = "print_jobs";
const char kKeyShortcutsFeature[] = "key_shortcuts";
const char kRecorderFeature[] = "recorder";

const char kBlockedDisableMode[] = "blocked";
const char kHiddenDisableMode[] = "hidden";

const char kSystemFeaturesDisableListHistogram[] =
    "Enterprise.SystemFeaturesDisableList";

SystemFeaturesDisableListPolicyHandler::SystemFeaturesDisableListPolicyHandler()
    : policy::ListPolicyHandler(key::kSystemFeaturesDisableList,
                                base::Value::Type::STRING) {}

SystemFeaturesDisableListPolicyHandler::
    ~SystemFeaturesDisableListPolicyHandler() = default;

void SystemFeaturesDisableListPolicyHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(policy_prefs::kSystemFeaturesDisableList);
  registry->RegisterStringPref(policy_prefs::kSystemFeaturesDisableMode,
                               kBlockedDisableMode);
}

SystemFeature SystemFeaturesDisableListPolicyHandler::GetSystemFeatureFromAppId(
    const std::string& app_id) {
  if (app_id == web_app::kCanvasAppId) {
    return SystemFeature::kCanvas;
  }
  return SystemFeature::kUnknownSystemFeature;
}

bool SystemFeaturesDisableListPolicyHandler::IsSystemFeatureDisabled(
    SystemFeature feature,
    PrefService* const pref_service) {
  if (!pref_service) {  // Sometimes it's not available in tests.
    return false;
  }

  const base::Value::List& disabled_system_features =
      pref_service->GetList(policy::policy_prefs::kSystemFeaturesDisableList);

  return base::Contains(disabled_system_features,
                        base::Value(static_cast<int>(feature)));
}

void SystemFeaturesDisableListPolicyHandler::ApplyList(
    base::Value::List filtered_list,
    PrefValueMap* prefs) {
  base::Value::List enums_list;
  base::Value* old_list = nullptr;
  prefs->GetValue(policy_prefs::kSystemFeaturesDisableList, &old_list);

  for (const auto& element : filtered_list) {
    SystemFeature feature = ConvertToEnum(element.GetString());
    enums_list.Append(static_cast<int>(feature));

    if (!old_list || !base::Contains(old_list->GetList(),
                                     base::Value(static_cast<int>(feature)))) {
      base::UmaHistogramEnumeration(kSystemFeaturesDisableListHistogram,
                                    feature);
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool os_settings_disabled = base::Contains(
      enums_list, base::Value(static_cast<int>(SystemFeature::kOsSettings)));
  prefs->SetBoolean(ash::prefs::kOsSettingsEnabled, !os_settings_disabled);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  prefs->SetValue(policy_prefs::kSystemFeaturesDisableList,
                  base::Value(std::move(enums_list)));
}

SystemFeature SystemFeaturesDisableListPolicyHandler::ConvertToEnum(
    const std::string& system_feature) {
  if (system_feature == kCameraFeature) {
    return SystemFeature::kCamera;
  }
  if (system_feature == kOsSettingsFeature) {
    return SystemFeature::kOsSettings;
  }
  if (system_feature == kBrowserSettingsFeature) {
    return SystemFeature::kBrowserSettings;
  }
  if (system_feature == kScanningFeature) {
    return SystemFeature::kScanning;
  }
  if (system_feature == kWebStoreFeature) {
    return SystemFeature::kWebStore;
  }
  if (system_feature == kCanvasFeature) {
    return SystemFeature::kCanvas;
  }
  if (system_feature == kExploreFeature) {
    return SystemFeature::kExplore;
  }
  if (system_feature == kCroshFeature) {
    return SystemFeature::kCrosh;
  }
  if (system_feature == kTerminalFeature) {
    return SystemFeature::kTerminal;
  }
  if (system_feature == kGalleryFeature) {
    return SystemFeature::kGallery;
  }
  if (system_feature == kPrintJobsFeature) {
    return SystemFeature::kPrintJobs;
  }
  if (system_feature == kKeyShortcutsFeature) {
    return SystemFeature::kKeyShortcuts;
  }
  if (system_feature == kRecorderFeature) {
    return SystemFeature::kRecorder;
  }
  LOG(ERROR) << "Unsupported system feature: " << system_feature;
  return SystemFeature::kUnknownSystemFeature;
}

}  // namespace policy

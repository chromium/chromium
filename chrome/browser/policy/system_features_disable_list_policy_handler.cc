// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "system_features_disable_list_policy_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/web_app_id_constants.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"

static_assert(BUILDFLAG(IS_CHROMEOS));

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
const char kGmailFeature[] = "gmail";
const char kGoogleDocsFeature[] = "google_docs";
const char kGoogleSlidesFeature[] = "google_slides";
const char kGoogleSheetsFeature[] = "google_sheets";
const char kGoogleDriveFeature[] = "google_drive";
const char kGoogleKeepFeature[] = "google_keep";
const char kGoogleCalendarFeature[] = "google_calendar";
const char kGoogleChatFeature[] = "google_chat";
const char kYoutubeFeature[] = "youtube";
const char kGoogleMapsFeature[] = "google_maps";
const char kCalculatorFeature[] = "calculator";
const char kTextEditorFeature[] = "text_editor";

const char kSystemFeaturesDisableListHistogram[] =
    "Enterprise.SystemFeaturesDisableList";

SystemFeaturesDisableListPolicyHandler::SystemFeaturesDisableListPolicyHandler()
    : policy::ListPolicyHandler(key::kSystemFeaturesDisableList,
                                base::Value::Type::STRING) {}

SystemFeaturesDisableListPolicyHandler::
    ~SystemFeaturesDisableListPolicyHandler() = default;

SystemFeature SystemFeaturesDisableListPolicyHandler::GetSystemFeatureFromAppId(
    const std::string& app_id) {
  if (app_id == ash::kCanvasAppId) {
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

  bool os_settings_disabled = base::Contains(
      enums_list, base::Value(static_cast<int>(SystemFeature::kOsSettings)));
  prefs->SetBoolean(ash::prefs::kOsSettingsEnabled, !os_settings_disabled);
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
  if (system_feature == kGmailFeature) {
    return SystemFeature::kGmail;
  }
  if (system_feature == kGoogleDocsFeature) {
    return SystemFeature::kGoogleDocs;
  }
  if (system_feature == kGoogleSlidesFeature) {
    return SystemFeature::kGoogleSlides;
  }
  if (system_feature == kGoogleSheetsFeature) {
    return SystemFeature::kGoogleSheets;
  }
  if (system_feature == kGoogleDriveFeature) {
    return SystemFeature::kGoogleDrive;
  }
  if (system_feature == kGoogleKeepFeature) {
    return SystemFeature::kGoogleKeep;
  }
  if (system_feature == kGoogleCalendarFeature) {
    return SystemFeature::kGoogleCalendar;
  }
  if (system_feature == kGoogleChatFeature) {
    return SystemFeature::kGoogleChat;
  }
  if (system_feature == kYoutubeFeature) {
    return SystemFeature::kYoutube;
  }
  if (system_feature == kGoogleMapsFeature) {
    return SystemFeature::kGoogleMaps;
  }
  if (system_feature == kCalculatorFeature) {
    return SystemFeature::kCalculator;
  }
  if (system_feature == kTextEditorFeature) {
    return SystemFeature::kTextEditor;
  }
  LOG(ERROR) << "Unsupported system feature: " << system_feature;
  return SystemFeature::kUnknownSystemFeature;
}

}  // namespace policy

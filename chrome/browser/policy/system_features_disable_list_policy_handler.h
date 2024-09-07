// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_SYSTEM_FEATURES_DISABLE_LIST_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_SYSTEM_FEATURES_DISABLE_LIST_POLICY_HANDLER_H_

#include <memory>

#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/prefs/pref_service.h"

class PrefValueMap;
class PrefRegistrySimple;

namespace policy {

// A system feature that can be disabled by SystemFeaturesDisableList policy.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SystemFeature : int {
  kUnknownSystemFeature = 0,
  kCamera = 1,                // The camera chrome app on ChromeOS.
  kBrowserSettings = 2,       // Browser settings.
  kOsSettings = 3,            // The settings feature on ChromeOS.
  kScanning = 4,              // The scan SWA on ChromeOS.
  kWebStore = 5,              // The web store chrome app on ChromeOS.
  kCanvas = 6,                // The canvas web app on ChromeOS.
  kGoogleNewsDeprecated = 7,  // The Google news app is no longer supported.
  kExplore = 8,               // The explore web app on ChromeOS.
  kCrosh = 9,                 // The ChromeOS shell.
  kTerminal = 10,             // The terminal client web app on ChromeOS.
  kGallery = 11,              // The gallery web app on ChromeOS.
  kPrintJobs = 12,            // The print jobs app on ChromeOS.
  kKeyShortcuts = 13,         // The Key Shortcuts app on ChromeOS.
  kRecorder = 14,             // The recorder app on ChromeOS.
  kMaxValue = kRecorder
};

// A disabling mode that decides the user experience when a system feature is
// added into SystemFeaturesDisableList policy.
enum class SystemFeatureDisableMode {
  kUnknownDisableMode = 0,
  kBlocked = 1,  // The disabled feature is blocked.
  kHidden = 2,   // The disabled feature is blocked and hidden.
  kMaxValue = kHidden
};

extern const char kCameraFeature[];
extern const char kBrowserSettingsFeature[];
extern const char kOsSettingsFeature[];
extern const char kScanningFeature[];
extern const char kWebStoreFeature[];
extern const char kCanvasFeature[];
extern const char kExploreFeature[];
extern const char kCroshFeature[];
extern const char kTerminalFeature[];
extern const char kGalleryFeature[];
extern const char kPrintJobsFeature[];
extern const char kKeyShortcutsFeature[];
extern const char kRecorderFeature[];

extern const char kBlockedDisableMode[];
extern const char kHiddenDisableMode[];

extern const char kSystemFeaturesDisableListHistogram[];

class SystemFeaturesDisableListPolicyHandler
    : public policy::ListPolicyHandler {
 public:
  SystemFeaturesDisableListPolicyHandler();
  ~SystemFeaturesDisableListPolicyHandler() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static SystemFeature GetSystemFeatureFromAppId(const std::string& app_id);
  static bool IsSystemFeatureDisabled(SystemFeature feature,
                                      PrefService* const pref_service);

 protected:
  // ListPolicyHandler:
  void ApplyList(base::Value::List filtered_list, PrefValueMap* prefs) override;

 private:
  SystemFeature ConvertToEnum(const std::string& system_feature);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_SYSTEM_FEATURES_DISABLE_LIST_POLICY_HANDLER_H_

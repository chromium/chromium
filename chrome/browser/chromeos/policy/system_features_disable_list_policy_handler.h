// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_FEATURES_DISABLE_LIST_POLICY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_FEATURES_DISABLE_LIST_POLICY_HANDLER_H_

#include <memory>

#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;
class PrefRegistrySimple;

namespace policy {

// A system feature that can be disabled by SystemFeaturesDisableList policy.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum SystemFeature {
  UNKNOWN_SYSTEM_FEATURE = 0,
  CAMERA = 1,            // The camera chrome app on Chrome OS.
  BROWSER_SETTINGS = 2,  // Browser settings.
  OS_SETTINGS = 3,       // The settings feature on Chrome OS.
  SCANNING = 4,          // The scan SWA on Chrome OS.
  kMaxValue = SCANNING
};

extern const char kCameraFeature[];
extern const char kBrowserSettingsFeature[];
extern const char kOsSettingsFeature[];
extern const char kScanningFeature[];

extern const char kSystemFeaturesDisableListHistogram[];

class SystemFeaturesDisableListPolicyHandler
    : public policy::ListPolicyHandler {
 public:
  SystemFeaturesDisableListPolicyHandler();
  ~SystemFeaturesDisableListPolicyHandler() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // ListPolicyHandler:
  void ApplyList(base::Value filtered_list, PrefValueMap* prefs) override;

 private:
  SystemFeature ConvertToEnum(const std::string& system_feature);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_FEATURES_DISABLE_LIST_POLICY_HANDLER_H_

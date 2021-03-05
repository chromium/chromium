// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_CONFIGURATION_KEYS_H_
#define CHROME_BROWSER_ASH_LOGIN_CONFIGURATION_KEYS_H_

#include "base/values.h"

namespace chromeos {
namespace configuration {
// Configuration keys that are used to automate OOBE screens go here.
// Please keep keys grouped by screens and ordered according to OOBE flow.

extern const char kSkipHIDDetection[];

extern const char kLanguage[];
extern const char kInputMethod[];
extern const char kWelcomeNext[];
extern const char kEnableDemoMode[];

extern const char kDemoModePreferencesNext[];

extern const char kNetworkSelectGUID[];
extern const char kNetworkOfflineDemo[];
extern const char kNetworkUseConnected[];

extern const char kDeviceRequisition[];

extern const char kEULASendUsageStatistics[];
extern const char kEULAAutoAccept[];

extern const char kArcTosAutoAccept[];

extern const char kUpdateSkipUpdate[];

extern const char kWizardAutoEnroll[];

extern const char kRestoreAfterRollback[];
extern const char kEnrollmentToken[];
extern const char kEnrollmentAssetId[];
extern const char kEnrollmentLocation[];
extern const char kEnrollmentAutoAttributes[];

enum class ConfigurationHandlerSide : unsigned int {
  HANDLER_JS,    // Handled by JS code
  HANDLER_CPP,   // Handled by C++ code
  HANDLER_BOTH,  // Used in both JS and C++ code
  HANDLER_DOC    // Not used by code, serves for documentation purposes only.
};

// Checks if configuration is valid (all fields have correct types, no extra
// fields).
bool ValidateConfiguration(const base::Value& configuration);

// Copies only fields handled by particular `side` from `configuration` to
// `filtered_result`.
void FilterConfiguration(const base::Value& configuration,
                         ConfigurationHandlerSide side,
                         base::Value& filtered_result);
}  // namespace configuration
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_CONFIGURATION_KEYS_H_

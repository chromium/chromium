// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_CONFIGURATION_KEYS_H_
#define CHROME_BROWSER_ASH_LOGIN_CONFIGURATION_KEYS_H_

#include "base/values.h"

namespace ash {
namespace configuration {
// Configuration keys that are used to automate OOBE screens go here.
// Please keep keys grouped by screens and ordered according to OOBE flow.

extern const char kLanguage[];
extern const char kInputMethod[];
extern const char kWelcomeNext[];
extern const char kEnableDemoMode[];

extern const char kDemoModePreferencesNext[];

extern const char kNetworkSelectGUID[];
extern const char kNetworkOfflineDemo[];
extern const char kNetworkUseConnected[];
extern const char kNetworkConfig[];

extern const char kDeviceRequisition[];

extern const char kEULASendUsageStatistics[];
extern const char kEULAAutoAccept[];

extern const char kArcTosAutoAccept[];

extern const char kRestoreAfterRollback[];
extern const char kEnrollmentAssetId[];
extern const char kEnrollmentLocation[];
extern const char kEnrollmentAutoAttributes[];
extern const char kEnrollmentToken[];

extern const char kSource[];

enum class ConfigurationHandlerSide : unsigned int {
  HANDLER_JS,    // Handled by JS code
  HANDLER_CPP,   // Handled by C++ code
  HANDLER_BOTH,  // Used in both JS and C++ code
  HANDLER_DOC    // Not used by code, serves for documentation purposes only.
};

// Checks if configuration is valid (all fields have correct types, no extra
// fields).
bool ValidateConfiguration(const base::Value::Dict& configuration);

// Returns a dictionary with only fields handled by particular `side` from
// `configuration`.
base::Value::Dict FilterConfiguration(const base::Value::Dict& configuration,
                                      ConfigurationHandlerSide side);
}  // namespace configuration
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_CONFIGURATION_KEYS_H_

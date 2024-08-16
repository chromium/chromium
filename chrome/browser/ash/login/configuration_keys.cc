// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/configuration_keys.h"

#include "base/logging.h"

namespace ash {
namespace configuration {

// Configuration keys that are used to automate OOBE screens go here.
// Please keep keys grouped by screens and ordered according to OOBE flow.
// All keys should be listed here, even if they are used in JS code only.
// These keys are used in
// chrome/browser/resources/chromeos/login/components/oobe_types.js

// == Welcome screen:

// Boolean value indicating if "Next" button on welcome screen is pressed
// automatically.
const char kWelcomeNext[] = "welcomeNext";

// String value that contains preferred input method.
const char kInputMethod[] = "inputMethod";

// String value that contains preferred input method.
const char kLanguage[] = "language";

// Boolean value indicating if device should automatically run the demo mode
// setup flow.
const char kEnableDemoMode[] = "enableDemoMode";

// == Demo mode preferences:

// Boolean value indicating if "Ok" button on Demo mode prefs screen is pressed
// automatically.
const char kDemoModePreferencesNext[] = "demoPreferencesNext";

// == Network screen:

// String value specifying GUID of the network that would be automatically
// selected.
const char kNetworkSelectGUID[] = "networkSelectGuid";

// Boolean value indicating if "Offline demo mode" should be automatically
// selected.
const char kNetworkOfflineDemo[] = "networkOfflineDemo";

// Boolean value specifying that the first connected network would be
// selected automatically.
const char kNetworkUseConnected[] = "networkUseConnected";

// String that holds network configuration preserved during rollback.
const char kNetworkConfig[] = "networkConfig";


// == EULA screen:

// Boolean value indicating if device should send usage statistics.
const char kEULASendUsageStatistics[] = "eulaSendStatistics";

// Boolean value indicating if the EULA is automatically accepted.
const char kEULAAutoAccept[] = "eulaAutoAccept";

// ARC++ TOS screen:

// Boolean value indicating if ARC++ Terms of service should be accepted
// automatically.
const char kArcTosAutoAccept[] = "arcTosAutoAccept";

// == Wizard controller:

// String value, containing device requisition parameter.
const char kDeviceRequisition[] = "deviceRequisition";

// == Enrollment screen

// Boolean value, indicates that device was enrolled before rollback.
const char kRestoreAfterRollback[] = "enrollmentRestoreAfterRollback";

// String value indicating what value would be propagated to Asset ID field
// on Device Attributes step.
const char kEnrollmentAssetId[] = "enrollmentAssetId";

// String value indicating what value would be propagated to Location field
// on Device Attributes step.
const char kEnrollmentLocation[] = "enrollmentLocation";

// Boolean value, controls if device attributes step should proceed with preset
// values.
const char kEnrollmentAutoAttributes[] = "enrollmentAutoAttributes";

// String value, contains enrollment token (currently only used for Flex Auto
// Enrollment).
const char kEnrollmentToken[] = "enrollmentToken";

// String value, indicates origin of OOBE config (i.e. what agent/purpose
// created the OOBE config and put it on the device).
// Currently used values are:
// - "REMOTE_DEPLOYMENT"
// - "PACKAGING_TOOL"
const char kSource[] = "source";

using ValueType = base::Value::Type;

constexpr struct {
  const char* key;
  ValueType type;
  ConfigurationHandlerSide side;
} kAllConfigurationKeys[] = {
    {kWelcomeNext, ValueType::BOOLEAN, ConfigurationHandlerSide::HANDLER_JS},
    {kLanguage, ValueType::STRING, ConfigurationHandlerSide::HANDLER_JS},
    {kInputMethod, ValueType::STRING, ConfigurationHandlerSide::HANDLER_JS},
    {kNetworkSelectGUID, ValueType::STRING,
     ConfigurationHandlerSide::HANDLER_JS},
    {kNetworkUseConnected, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_JS},
    {kNetworkConfig, ValueType::STRING, ConfigurationHandlerSide::HANDLER_CPP},
    {kEULASendUsageStatistics, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_JS},
    {kEULAAutoAccept, ValueType::BOOLEAN, ConfigurationHandlerSide::HANDLER_JS},
    {kRestoreAfterRollback, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_CPP},
    {kDeviceRequisition, ValueType::STRING,
     ConfigurationHandlerSide::HANDLER_CPP},
    {kEnrollmentLocation, ValueType::STRING,
     ConfigurationHandlerSide::HANDLER_CPP},
    {kEnrollmentLocation, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_CPP},
    {kEnableDemoMode, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_BOTH},
    {kDemoModePreferencesNext, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_JS},
    {kNetworkOfflineDemo, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_JS},
    {kArcTosAutoAccept, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_BOTH},
    {kEnrollmentToken, ValueType::STRING,
     ConfigurationHandlerSide::HANDLER_CPP},
    {kSource, ValueType::STRING, ConfigurationHandlerSide::HANDLER_CPP},
    {"desc", ValueType::STRING, ConfigurationHandlerSide::HANDLER_DOC},
    {"testValue", ValueType::STRING, ConfigurationHandlerSide::HANDLER_BOTH},
};

bool ValidateConfiguration(const base::Value::Dict& configuration) {
  base::Value::Dict clone = configuration.Clone();
  bool valid = true;
  for (const auto& key : kAllConfigurationKeys) {
    auto* value = clone.Find(key.key);
    if (value) {
      if (value->type() != key.type) {
        valid = false;
        LOG(ERROR) << "Invalid configuration: key " << key.key
                   << " type is invalid";
      }
      clone.Remove(key.key);
    }
  }
  for (const auto item : clone)
    LOG(WARNING) << "Unknown configuration key " << item.first;
  return valid;
}

base::Value::Dict FilterConfiguration(const base::Value::Dict& configuration,
                                      ConfigurationHandlerSide side) {
  DCHECK(side == ConfigurationHandlerSide::HANDLER_CPP ||
         side == ConfigurationHandlerSide::HANDLER_JS);
  base::Value::Dict filtered_result;
  for (const auto& key : kAllConfigurationKeys) {
    if (key.side == side ||
        key.side == ConfigurationHandlerSide::HANDLER_BOTH) {
      auto* value = configuration.Find(key.key);
      if (value) {
        filtered_result.Set(key.key, value->Clone());
      }
    }
  }
  return filtered_result;
}

}  // namespace configuration
}  // namespace ash

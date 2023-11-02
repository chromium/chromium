// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_DECODER_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_DECODER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_management {
class ChromeDeviceSettingsProto;
}

namespace base {
class Value;
}

namespace policy {

class ExternalDataManager;
class PolicyMap;

// A pattern for validating hostnames. Used for the validation of
// DeviceLoginScreenDomainAutoComplete policy.
extern const char hostNameRegex[];

// Decodes a JSON string to a base::Value and validates it against the schema
// defined in policy_templates.json for the policy named |policy_name|. Unknown
// properties are dropped. Returns nullptr if the input cannot be parsed as
// valid JSON string or doesn't comply with the declared schema (e.g. mismatched
// type, missing required field, etc.). Any warning or error messages from the
// decoding and schema validation process are stored in |error|.
absl::optional<base::Value> DecodeJsonStringAndNormalize(
    const std::string& json_string,
    const std::string& policy_name,
    std::string* error);

// Decodes device policy in ChromeDeviceSettingsProto representation into the a
// PolicyMap.
void DecodeDevicePolicy(
    const enterprise_management::ChromeDeviceSettingsProto& policy,
    base::WeakPtr<ExternalDataManager> external_data_manager,
    PolicyMap* policies);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_DECODER_H_

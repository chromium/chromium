// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_DECODER_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_DECODER_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/values.h"

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

// The result of decoding a JSON string to a base::Value and validating it
// against the schema defined in POLICY_NAME.yaml in
// //components/policy/resources/templates/policy_definitions/.
//
// If the decoding returned errors that do not invalidate the schema,
// `non_fatal_errors` will contain these errors, as they might be of interest
// for callers.
struct DecodeJsonResult {
  DecodeJsonResult() = delete;
  ~DecodeJsonResult();

  DecodeJsonResult(base::Value decoded_json,
                   std::optional<std::string> non_fatal_errors);

  DecodeJsonResult(DecodeJsonResult&& other);
  DecodeJsonResult& operator=(DecodeJsonResult&& other);

  base::Value decoded_json;
  std::optional<std::string> non_fatal_errors;
};

using DecodeJsonError = std::string;

// Decodes a JSON string to a base::Value and validates it against the schema
// defined in POLICY_NAME.yaml in
// //components/policy/resources/templates/policy_definitions/.
//
// Unknown properties are dropped. Returns a `DecodeJsonResult` object if the
// input can be parsed as valid JSON string, and complies with the declared
// schema. If the input cannot be parsed as valid JSON string or doesn't comply
// with the declared schema, returns a error message instead.
base::expected<DecodeJsonResult, DecodeJsonError> DecodeJsonStringAndNormalize(
    const std::string& json_string,
    const std::string& policy_name);

// Decodes device policy in ChromeDeviceSettingsProto representation into the a
// PolicyMap.
void DecodeDevicePolicy(
    const enterprise_management::ChromeDeviceSettingsProto& policy,
    base::WeakPtr<ExternalDataManager> external_data_manager,
    PolicyMap* policies);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_DECODER_H_

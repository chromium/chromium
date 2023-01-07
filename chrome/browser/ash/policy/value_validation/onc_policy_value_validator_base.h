// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_VALUE_VALIDATION_ONC_POLICY_VALUE_VALIDATOR_BASE_H_
#define CHROME_BROWSER_ASH_POLICY_VALUE_VALIDATION_ONC_POLICY_VALUE_VALIDATOR_BASE_H_

#include "components/policy/core/common/cloud/policy_value_validator.h"

#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/components/onc/onc_validator.h"
#include "components/onc/onc_constants.h"

namespace policy {

// Base class for validating OpenNetworkConfiguration (ONC) device and network
// policy values. Subclasses will implement the access to the concrete policy
// fields.
template <typename PayloadProto>
class ONCPolicyValueValidatorBase : public PolicyValueValidator<PayloadProto> {
 public:
  ONCPolicyValueValidatorBase(const std::string& onc_policy_name,
                              ::onc::ONCSource source)
      : policy_name_(onc_policy_name), source_(source) {}

  ONCPolicyValueValidatorBase(const ONCPolicyValueValidatorBase&) = delete;
  ONCPolicyValueValidatorBase& operator=(const ONCPolicyValueValidatorBase&) =
      delete;

  virtual ~ONCPolicyValueValidatorBase() {}

  // PolicyValueValidator:
  bool ValidateValues(
      const PayloadProto& policy_payload,
      std::vector<ValueValidationIssue>* out_validation_issues) const override {
    absl::optional<std::string> onc_string =
        GetONCStringFromPayload(policy_payload);

    if (!onc_string.has_value())
      return true;

    base::Value root_dict =
        chromeos::onc::ReadDictionaryFromJson(onc_string.value());
    if (!root_dict.is_dict()) {
      out_validation_issues->push_back({policy_name_,
                                        ValueValidationIssue::Severity::kError,
                                        "JSON parse error."});
      return false;
    }

    chromeos::onc::Validator validator(
        false,  // Ignore unknown fields.
        false,  // Ignore invalid recommended field names.
        true,   // Fail on missing fields.
        true,   // Validate for managed ONC.
        true);  // Log warnings.
    validator.SetOncSource(source_);
    chromeos::onc::Validator::Result validation_result;
    validator.ValidateAndRepairObject(
        &chromeos::onc::kToplevelConfigurationSignature, root_dict,
        &validation_result);

    bool error_found = false;
    for (const chromeos::onc::Validator::ValidationIssue& issue :
         validator.validation_issues()) {
      ValueValidationIssue::Severity severity =
          issue.is_error ? ValueValidationIssue::Severity::kError
                         : ValueValidationIssue::Severity::kWarning;
      out_validation_issues->push_back({policy_name_, severity, issue.message});
      error_found |= issue.is_error;
    }
    return !error_found;
  }

 protected:
  virtual absl::optional<std::string> GetONCStringFromPayload(
      const PayloadProto& policy_payload) const = 0;

 private:
  const std::string policy_name_;
  const ::onc::ONCSource source_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_VALUE_VALIDATION_ONC_POLICY_VALUE_VALIDATOR_BASE_H_

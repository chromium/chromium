// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_overrides_policy_handler.h"

#include "base/values.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/first_party_sets_handler.h"

namespace first_party_sets {

namespace {

using ParseError = content::FirstPartySetsHandler::ParseError;
using ParseWarning = content::FirstPartySetsHandler::ParseWarning;

std::string ParseErrorTypeToString(
    content::FirstPartySetsHandler::ParseErrorType error) {
  switch (error) {
    case content::FirstPartySetsHandler::ParseErrorType::kInvalidType:
      return "This set is an invalid type.";
    case content::FirstPartySetsHandler::ParseErrorType::kInvalidOrigin:
      return "This set contains an invalid origin.";
    case content::FirstPartySetsHandler::ParseErrorType::kNonHttpsScheme:
      return "This set contains a non-HTTPS origin.";
    case content::FirstPartySetsHandler::ParseErrorType::kInvalidDomain:
      return "This set contains an invalid registrable domain.";
    case content::FirstPartySetsHandler::ParseErrorType::kSingletonSet:
      return "This set doesn't contain any sites in its associatedSites list.";
    case content::FirstPartySetsHandler::ParseErrorType::kNonDisjointSets:
      return "This set contains a domain that also exists in another "
             "First-Party Set.";
    case content::FirstPartySetsHandler::ParseErrorType::kRepeatedDomain:
      return "This set contains more than one occurrence of the same domain.";
  }
}

std::string ParseWarningTypeToString(
    const content::FirstPartySetsHandler::ParseWarningType& warning) {
  switch (warning) {
    case content::FirstPartySetsHandler::ParseWarningType::
        kCctldKeyNotCanonical:
      return "This \"ccTLDs\" entry is ignored since this key is not in the "
             "set.";
    case content::FirstPartySetsHandler::ParseWarningType::
        kAliasNotCctldVariant:
      return "This \"ccTLD\" is ignored since it differs from its key by more "
             "than eTLD.";
  }
}

}  // namespace

FirstPartySetsOverridesPolicyHandler::FirstPartySetsOverridesPolicyHandler(
    const char* policy_name,
    const policy::Schema& schema)
    : policy::SchemaValidatingPolicyHandler(
          policy_name,
          schema.GetKnownProperty(policy_name),
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN) {}

FirstPartySetsOverridesPolicyHandler::~FirstPartySetsOverridesPolicyHandler() =
    default;

bool FirstPartySetsOverridesPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  std::unique_ptr<base::Value> policy_value;
  if (!policy::SchemaValidatingPolicyHandler::CheckAndGetValue(policies, errors,
                                                               &policy_value) ||
      !policy_value) {
    return false;
  }

  // Output error and return false if any of the sets provided in the
  // "replacements" or "additions" list are not valid First-Party Sets.
  auto [success, warnings] =
      content::FirstPartySetsHandler::ValidateEnterprisePolicy(
          policy_value->GetDict());

  // Output warnings that occur when parsing the policy.
  for (ParseWarning parse_warning : warnings) {
    errors->AddError(policy_name(), IDS_POLICY_SCHEMA_VALIDATION_ERROR,
                     ParseWarningTypeToString(parse_warning.type()),
                     parse_warning.path(),
                     policy::PolicyMap::MessageType::kWarning);
  }

  if (!success.has_value()) {
    ParseError parse_error = success.error();
    errors->AddError(policy_name(), IDS_POLICY_SCHEMA_VALIDATION_ERROR,
                     ParseErrorTypeToString(parse_error.type()),
                     parse_error.path());
    return false;
  }
  return true;
}

void FirstPartySetsOverridesPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> value;
  policy::SchemaValidatingPolicyHandler::CheckAndGetValue(policies, nullptr,
                                                          &value);
  prefs->SetValue(first_party_sets::kRelatedWebsiteSetsOverrides,
                  base::Value::FromUniquePtrValue(std::move(value)));
}

}  // namespace first_party_sets

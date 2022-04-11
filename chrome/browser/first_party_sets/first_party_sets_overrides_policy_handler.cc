// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_overrides_policy_handler.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "net/base/schemeful_site.h"

namespace first_party_sets {

namespace {

const char kFirstPartySetPolicyReplacementsField[] = "replacements";
const char kFirstPartySetPolicyAdditionsField[] = "additions";

// Converts a ParseError to an appropriate string to be outputted to enterprise
// administrators.
std::string ParseErrorToString(
    content::FirstPartySetsHandler::ParseError error) {
  switch (error) {
    case content::FirstPartySetsHandler::ParseError::kInvalidType:
      return "This set is an invalid type.";
    case content::FirstPartySetsHandler::ParseError::kInvalidOrigin:
      return "This set contains an invalid origin.";
    case content::FirstPartySetsHandler::ParseError::kSingletonSet:
      return "This set doesn't contain any sites in its members list.";
    case content::FirstPartySetsHandler::ParseError::kNonDisjointSets:
      return "This set contains a domain that also exists in another "
             "First-Party Set.";
    case content::FirstPartySetsHandler::ParseError::kRepeatedDomain:
      return "This set contains more than one occurrence of the same domain.";
  }
}

// Converts a PolicySetType to an string describing the type.
const char* SetTypeToString(
    content::FirstPartySetsHandler::PolicySetType set_type) {
  switch (set_type) {
    case content::FirstPartySetsHandler::PolicySetType::kReplacement:
      return kFirstPartySetPolicyReplacementsField;
    case content::FirstPartySetsHandler::PolicySetType::kAddition:
      return kFirstPartySetPolicyAdditionsField;
  }
}

}  // namespace

FirstPartySetsOverridesPolicyHandler::FirstPartySetsOverridesPolicyHandler(
    const policy::Schema& schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kFirstPartySetsOverrides,
          schema.GetKnownProperty(policy::key::kFirstPartySetsOverrides),
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
  if (absl::optional<content::FirstPartySetsHandler::PolicyParsingError>
          maybe_error =
              content::FirstPartySetsHandler::ValidateEnterprisePolicy(
                  policy_value->GetDict());
      maybe_error.has_value()) {
    errors->AddError(policy_name(),
                     base::StringPrintf("%s.items[%d]",
                                        SetTypeToString(maybe_error->set_type),
                                        maybe_error->error_index),
                     ParseErrorToString(maybe_error->error));
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
  prefs->SetValue(first_party_sets::kFirstPartySetsOverrides,
                  base::Value::FromUniquePtrValue(std::move(value)));
}

}  // namespace first_party_sets

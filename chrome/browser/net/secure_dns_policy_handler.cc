// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_policy_handler.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/policy/managed_configuration_variables.h"
#include "chrome/browser/ash/policy/core/device_attributes_impl.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr int kMinDohSaltSize = 8;
constexpr int kMaxDohSaltSize = 32;

// Returns true if the policy `kDnsOverHttpsSalt` has a valid value, otherwise
// returns false. This method also evaluates if the combination of
// `kDnsOverHttpsTemplatesWithIdentifiers` and the `kDnsOverHttpsSalt` policy
// values is correct (i.e. both are set or both are unset). If an error occurs,
// the error message will be appended to `errors`.
bool CheckDnsOverHttpsSaltPolicy(const policy::PolicyMap& policies,
                                 policy::PolicyErrorMap* errors) {
  bool templates_set = false;

  if (policies.IsPolicySet(
          policy::key::kDnsOverHttpsTemplatesWithIdentifiers)) {
    const base::Value* templates =
        policies.GetValue(policy::key::kDnsOverHttpsTemplatesWithIdentifiers,
                          base::Value::Type::STRING);
    templates_set = templates && !templates->GetString().empty();
  }
  if (!policies.IsPolicySet(policy::key::kDnsOverHttpsSalt)) {
    if (templates_set) {
      errors->AddError(
          policy::key::kDnsOverHttpsTemplatesWithIdentifiers,
          IDS_POLICY_SECURE_DNS_TEMPLATES_WITH_IDENTIFIERS_IRRELEVANT_ERROR);
    }
    return false;
  }
  const base::Value* salt =
      policies.GetValueUnsafe(policy::key::kDnsOverHttpsSalt);

  if (!salt->is_string()) {
    errors->AddError(policy::key::kDnsOverHttpsSalt,
                     IDS_POLICY_SECURE_DNS_SALT_INVALID_ERROR);
    return false;
  }

  // The general case where the salt is set and empty is evaluated with the salt
  // size requirements checks.
  if (templates_set && salt->GetString().empty()) {
    errors->AddError(
        policy::key::kDnsOverHttpsTemplatesWithIdentifiers,
        IDS_POLICY_SECURE_DNS_TEMPLATES_WITH_IDENTIFIERS_IRRELEVANT_ERROR);
  }

  if (salt->GetString().size() < kMinDohSaltSize ||
      salt->GetString().size() > kMaxDohSaltSize) {
    errors->AddError(policy::key::kDnsOverHttpsSalt,
                     IDS_POLICY_SECURE_DNS_SALT_INVALID_SIZE_ERROR);
    return false;
  }

  if (!templates_set) {
    errors->AddError(policy::key::kDnsOverHttpsSalt,
                     IDS_POLICY_SECURE_DNS_SALT_IRRELEVANT_ERROR);
  }

  return true;
}
#endif

// Returns true if the policy `policy_name` has a valid template URI value,
// otherwise returns false. If an error occurs, the error message will be
// appended to `errors`.
bool CheckDnsOverHttpsTemplatePolicy(const policy::PolicyMap& policies,
                                     policy::PolicyErrorMap* errors,
                                     const std::string& policy_name,
                                     const base::StringPiece& mode) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* templates = policies.GetValueUnsafe(policy_name);

  if (!templates)
    return false;

  if (!templates->is_string()) {
    errors->AddError(policy_name, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::STRING));
    return false;
  }
  if (templates->GetString().empty())
    return false;

  const std::string& templates_str = templates->GetString();
  if (!net::DnsOverHttpsConfig::FromString(templates_str)) {
    errors->AddError(policy_name,
                     IDS_POLICY_SECURE_DNS_TEMPLATES_INVALID_ERROR);
  }
  return true;
}

}  // namespace

namespace policy {

SecureDnsPolicyHandler::SecureDnsPolicyHandler() {}

SecureDnsPolicyHandler::~SecureDnsPolicyHandler() {}

// Verifies if the combination of policies which set the secure DNS mode and
// the templates URI is valid. The templates URIs can be set via the cross
// platform policy "DnsOverHttpsTemplates" and, on Chrome OS only, via the
// policy "DnsOverHttpsTemplatesWithIdentifiers" which will override the cross
// platform policy if both are set.
bool SecureDnsPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                 PolicyErrorMap* errors) {
  bool mode_is_applicable = true;
  bool templates_is_applicable = true;
  std::string applicable_template_policy_name = key::kDnsOverHttpsTemplates;

  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* mode = policies.GetValueUnsafe(key::kDnsOverHttpsMode);
  base::StringPiece mode_str;
  if (!mode) {
    mode_is_applicable = false;
  } else if (!mode->is_string()) {
    errors->AddError(key::kDnsOverHttpsMode, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::STRING));
    mode_is_applicable = false;
  } else {
    // Mode is set and is a string.
    mode_str = mode->GetString();

    if (mode_str.size() == 0) {
      errors->AddError(key::kDnsOverHttpsMode, IDS_POLICY_NOT_SPECIFIED_ERROR);
      mode_is_applicable = false;
    } else if (!SecureDnsConfig::ParseMode(mode_str)) {
      errors->AddError(key::kDnsOverHttpsMode,
                       IDS_POLICY_INVALID_SECURE_DNS_MODE_ERROR);
      mode_is_applicable = false;
    }
  }

  is_templates_policy_valid_ = CheckDnsOverHttpsTemplatePolicy(
      policies, errors, key::kDnsOverHttpsTemplates, mode_str);
  templates_is_applicable = is_templates_policy_valid_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::features::IsDnsOverHttpsWithIdentifiersEnabled()) {
    bool templates_valid = CheckDnsOverHttpsTemplatePolicy(
        policies, errors, key::kDnsOverHttpsTemplatesWithIdentifiers, mode_str);
    bool salt_valid = CheckDnsOverHttpsSaltPolicy(policies, errors);
    is_templates_with_identifiers_policy_valid_ = templates_valid && salt_valid;
  }
  templates_is_applicable =
      is_templates_policy_valid_ || is_templates_with_identifiers_policy_valid_;

  if (is_templates_with_identifiers_policy_valid_) {
    applicable_template_policy_name =
        key::kDnsOverHttpsTemplatesWithIdentifiers;
  }
#endif
  if (IsTemplatesPolicyNotSpecified(templates_is_applicable, mode_str)) {
    errors->AddError(applicable_template_policy_name,
                     IDS_POLICY_SECURE_DNS_TEMPLATES_NOT_SPECIFIED_ERROR);
  }

  if (templates_is_applicable) {
    if (!mode) {
      errors->AddError(applicable_template_policy_name,
                       IDS_POLICY_SECURE_DNS_TEMPLATES_UNSET_MODE_ERROR);
    } else if (!mode_is_applicable) {
      errors->AddError(applicable_template_policy_name,
                       IDS_POLICY_SECURE_DNS_TEMPLATES_INVALID_MODE_ERROR);
    } else if (mode_str == SecureDnsConfig::kModeOff) {
      errors->AddError(applicable_template_policy_name,
                       IDS_POLICY_SECURE_DNS_TEMPLATES_IRRELEVANT_MODE_ERROR);
    }
  }
  return mode_is_applicable || templates_is_applicable;
}

void SecureDnsPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                 PrefValueMap* prefs) {
  const base::Value* mode =
      policies.GetValue(key::kDnsOverHttpsMode, base::Value::Type::STRING);
  base::StringPiece mode_str;
  if (mode) {
    mode_str = mode->GetString();
    prefs->SetString(prefs::kDnsOverHttpsMode,
                     SecureDnsConfig::ParseMode(mode_str)
                         ? std::string(mode_str)
                         : SecureDnsConfig::kModeOff);
  }

  const base::Value* templates =
      policies.GetValue(key::kDnsOverHttpsTemplates, base::Value::Type::STRING);

  // A templates not specified error means that the pref should be set blank.
  if (IsTemplatesPolicyNotSpecified(is_templates_policy_valid_, mode_str))
    prefs->SetString(prefs::kDnsOverHttpsTemplates, std::string());
  else if (is_templates_policy_valid_)
    prefs->SetString(prefs::kDnsOverHttpsTemplates, templates->GetString());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::features::IsDnsOverHttpsWithIdentifiersEnabled()) {
    const base::Value* templates_with_identifiers = policies.GetValue(
        key::kDnsOverHttpsTemplatesWithIdentifiers, base::Value::Type::STRING);
    const base::Value* salt =
        policies.GetValue(key::kDnsOverHttpsSalt, base::Value::Type::STRING);

    // A templates not specified error means that the pref should be set blank.
    if (IsTemplatesPolicyNotSpecified(
            is_templates_with_identifiers_policy_valid_, mode_str)) {
      prefs->SetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                       std::string());
      prefs->SetString(prefs::kDnsOverHttpsSalt, std::string());

    } else if (is_templates_with_identifiers_policy_valid_) {
      prefs->SetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                       templates_with_identifiers->GetString());
      prefs->SetString(prefs::kDnsOverHttpsSalt, salt->GetString());
    }
  }
#endif
}

bool SecureDnsPolicyHandler::IsTemplatesPolicyNotSpecified(
    bool is_templates_policy_valid,
    base::StringPiece mode_str) {
  if (mode_str == SecureDnsConfig::kModeSecure)
    return !is_templates_policy_valid;

  return false;
}

}  // namespace policy

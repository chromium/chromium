// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_policy_handler.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/util.h"

namespace policy {

SecureDnsPolicyHandler::SecureDnsPolicyHandler() {}

SecureDnsPolicyHandler::~SecureDnsPolicyHandler() {}

bool SecureDnsPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                 PolicyErrorMap* errors) {
  bool mode_is_applicable = true;
  bool templates_is_applicable = true;

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

  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* templates =
      policies.GetValueUnsafe(key::kDnsOverHttpsTemplates);
  if (IsTemplatesPolicyNotSpecified(templates, mode_str)) {
    errors->AddError(key::kDnsOverHttpsTemplates,
                     IDS_POLICY_SECURE_DNS_TEMPLATES_NOT_SPECIFIED_ERROR);
  } else if (!templates) {
    templates_is_applicable = false;
  } else if (!templates->is_string()) {
    errors->AddError(key::kDnsOverHttpsTemplates, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::STRING));
    templates_is_applicable = false;
  } else {
    const std::string& templates_str = templates->GetString();

    if (!templates_str.empty()) {
      if (!mode) {
        errors->AddError(key::kDnsOverHttpsTemplates,
                         IDS_POLICY_SECURE_DNS_TEMPLATES_UNSET_MODE_ERROR);
      } else if (!mode_is_applicable) {
        errors->AddError(key::kDnsOverHttpsTemplates,
                         IDS_POLICY_SECURE_DNS_TEMPLATES_INVALID_MODE_ERROR);
      } else if (mode_str == SecureDnsConfig::kModeOff) {
        errors->AddError(key::kDnsOverHttpsTemplates,
                         IDS_POLICY_SECURE_DNS_TEMPLATES_IRRELEVANT_MODE_ERROR);
      } else if (!net::DnsOverHttpsConfig::FromString(templates_str)) {
        errors->AddError(key::kDnsOverHttpsTemplates,
                         IDS_POLICY_SECURE_DNS_TEMPLATES_INVALID_ERROR);
      }
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
  if (IsTemplatesPolicyNotSpecified(templates, mode_str))
    prefs->SetString(prefs::kDnsOverHttpsTemplates, std::string());
  else if (ShouldSetTemplatesPref(templates))
    prefs->SetString(prefs::kDnsOverHttpsTemplates, templates->GetString());
}

bool SecureDnsPolicyHandler::IsTemplatesPolicyNotSpecified(
    const base::Value* templates,
    base::StringPiece mode_str) {
  if (mode_str == SecureDnsConfig::kModeSecure) {
    if (!templates || !templates->is_string())
      return true;

    DCHECK(templates->is_string());
    base::StringPiece templates_str = templates->GetString();

    if (templates_str.size() == 0)
      return true;
  }

  return false;
}

bool SecureDnsPolicyHandler::ShouldSetTemplatesPref(
    const base::Value* templates) {
  return templates != nullptr;
}

}  // namespace policy

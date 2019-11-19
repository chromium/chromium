// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_policy_handler.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

SecureDnsPolicyHandler::SecureDnsPolicyHandler() {}

SecureDnsPolicyHandler::~SecureDnsPolicyHandler() {}

bool SecureDnsPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                 PolicyErrorMap* errors) {
  bool mode_is_applicable = true;
  bool templates_is_applicable = true;

  const base::Value* mode = policies.GetValue(key::kDnsOverHttpsMode);
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
    } else if (mode_str == chrome_browser_net::kDnsOverHttpsModeSecure) {
      errors->AddError(key::kDnsOverHttpsMode,
                       IDS_POLICY_SECURE_DNS_MODE_NOT_SUPPORTED_ERROR);
    } else if (mode_str != chrome_browser_net::kDnsOverHttpsModeOff &&
               mode_str != chrome_browser_net::kDnsOverHttpsModeAutomatic) {
      errors->AddError(key::kDnsOverHttpsMode,
                       IDS_POLICY_INVALID_SECURE_DNS_MODE_ERROR);
      mode_is_applicable = false;
    }
  }

  const base::Value* templates = policies.GetValue(key::kDnsOverHttpsTemplates);
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
    // Templates is set and is a string.
    base::StringPiece templates_str = templates->GetString();

    if (templates_str.size() != 0 && !mode) {
      errors->AddError(key::kDnsOverHttpsTemplates,
                       IDS_POLICY_SECURE_DNS_TEMPLATES_UNSET_MODE_ERROR);
    } else if (templates_str.size() != 0 && !mode_is_applicable) {
      errors->AddError(key::kDnsOverHttpsTemplates,
                       IDS_POLICY_SECURE_DNS_TEMPLATES_INVALID_MODE_ERROR);
    } else if (templates_str.size() != 0 &&
               mode_str == chrome_browser_net::kDnsOverHttpsModeOff) {
      errors->AddError(key::kDnsOverHttpsTemplates,
                       IDS_POLICY_SECURE_DNS_TEMPLATES_IRRELEVANT_MODE_ERROR);
    } else if (IsTemplatesPolicyInvalid(templates_str)) {
      errors->AddError(key::kDnsOverHttpsTemplates,
                       IDS_POLICY_SECURE_DNS_TEMPLATES_INVALID_ERROR);
    }
  }

  return mode_is_applicable || templates_is_applicable;
}

void SecureDnsPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                 PrefValueMap* prefs) {
  const base::Value* mode = policies.GetValue(key::kDnsOverHttpsMode);
  base::StringPiece mode_str;
  if (mode && mode->is_string()) {
    mode_str = mode->GetString();
    // TODO(http://crbug.com/955454): Include secure in conditional when
    // support is implemented.
    if (mode_str == chrome_browser_net::kDnsOverHttpsModeAutomatic) {
      prefs->SetString(prefs::kDnsOverHttpsMode, mode_str.as_string());
    } else {
      // Captures "off" and "secure".
      prefs->SetString(prefs::kDnsOverHttpsMode,
                       chrome_browser_net::kDnsOverHttpsModeOff);
    }
  }

  const base::Value* templates = policies.GetValue(key::kDnsOverHttpsTemplates);

  // A templates not specified error means that the pref should be set blank.
  if (IsTemplatesPolicyNotSpecified(templates, mode_str))
    prefs->SetString(prefs::kDnsOverHttpsTemplates, std::string());
  else if (ShouldSetTemplatesPref(templates))
    prefs->SetString(prefs::kDnsOverHttpsTemplates, templates->GetString());
}

bool SecureDnsPolicyHandler::IsTemplatesPolicyNotSpecified(
    const base::Value* templates,
    base::StringPiece mode_str) {
  if (mode_str == chrome_browser_net::kDnsOverHttpsModeSecure) {
    if (!templates)
      return true;

    if (!templates->is_string())
      return true;

    base::StringPiece templates_str = templates->GetString();

    if (templates_str.size() == 0)
      return true;
  }

  return false;
}

bool SecureDnsPolicyHandler::IsTemplatesPolicyInvalid(
    const base::StringPiece templates_str) {
  // server_method is unused, it's just to satisfy IsValidDohTemplate()'s
  // function signature
  std::string server_method;
  for (const std::string& server_template :
       SplitString(templates_str, " ", base::TRIM_WHITESPACE,
                   base::SPLIT_WANT_NONEMPTY)) {
    if (!chrome_browser_net::IsValidDohTemplate(server_template,
                                                &server_method)) {
      return true;
    }
  }

  return false;
}

bool SecureDnsPolicyHandler::ShouldSetTemplatesPref(
    const base::Value* templates) {
  if (!templates)
    return false;

  if (!templates->is_string())
    return false;

  return true;
}

}  // namespace policy

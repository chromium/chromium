// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/gemini_enterprise_url_policy_handler.h"

#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace policy {

GeminiEnterpriseUrlPolicyHandler::GeminiEnterpriseUrlPolicyHandler()
    : SimplePolicyHandler(key::kGeminiEnterpriseUrl,
                          glic::prefs::kGlicGeminiEnterpriseUrl,
                          base::Value::Type::STRING) {}

GeminiEnterpriseUrlPolicyHandler::~GeminiEnterpriseUrlPolicyHandler() = default;

// static
bool GeminiEnterpriseUrlPolicyHandler::CheckUrl(const std::string& policy_name,
                                                const std::string& url,
                                                PolicyErrorMap* errors,
                                                PolicyErrorPath error_path) {
  const GURL gurl(url);
  // Embedded credentials are rejected because the URL is loaded in a
  // privileged WebContents.
  if (!gurl.is_valid() || gurl.has_username() || gurl.has_password()) {
    errors->AddError(policy_name, IDS_POLICY_INVALID_URL_ERROR,
                     std::move(error_path));
    return false;
  }
  if (!gurl.SchemeIs(url::kHttpsScheme)) {
    errors->AddError(policy_name, IDS_POLICY_URL_NOT_HTTPS_ERROR,
                     std::move(error_path));
    return false;
  }
  if (!geic::IsAllowedGeminiEnterpriseHost(gurl)) {
    errors->AddError(policy_name,
                     IDS_POLICY_GEMINI_ENTERPRISE_URL_HOST_NOT_ALLOWED_ERROR,
                     std::move(error_path));
    return false;
  }
  return true;
}

bool GeminiEnterpriseUrlPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  if (!SimplePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  // The base class accepts an unset policy and rejects any value that is not
  // a string, so a null value here means the policy is not set.
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::STRING);
  if (!value || value->GetString().empty()) {
    return true;
  }
  return CheckUrl(policy_name(), value->GetString(), errors);
}

}  // namespace policy

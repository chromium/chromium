// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_policy_handler.h"

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

namespace enterprise_reporting {

LegacyTechReportPolicyHandler::LegacyTechReportPolicyHandler()
    : policy::URLSchemeListPolicyHandler(
          policy::key::kLegacyTechReportAllowlist,
          kCloudLegacyTechReportAllowlist) {}

LegacyTechReportPolicyHandler::~LegacyTechReportPolicyHandler() = default;

bool LegacyTechReportPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!policies.IsPolicySet(policy_name())) {
    return true;
  }
  if (!URLSchemeListPolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  if (!policy::CloudOnlyPolicyHandler::CheckCloudOnlyPolicySettings(
          policy_name(), policies, errors)) {
    return false;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  const policy::PolicyMap::Entry* policy = policies.Get(policy_name());
  // If policy is set with the signed in account, it must be affiliated.
  if (policy->scope == policy::POLICY_SCOPE_USER &&
      !policies.IsUserAffiliated()) {
    errors->AddError(policy_name(), IDS_POLICY_USER_IS_NOT_AFFILIATED_ERROR);
    return false;
  }
#endif

  return true;
}

size_t LegacyTechReportPolicyHandler::max_items() {
  return 1000;
}

bool LegacyTechReportPolicyHandler::ValidatePolicyEntry(
    const std::string* policy) {
  url_matcher::util::FilterComponents components;
  if (!policy) {
    return false;
  }

  if (!url_matcher::util::FilterToComponents(
          *policy, &components.scheme, &components.host,
          &components.match_subdomains, &components.port, &components.path,
          &components.query)) {
    return false;
  }

  // Wildcard URL '*' and Wildcard host is not allowed.
  if (components.IsWildcard() || components.host == "") {
    return false;
  }

  // Scheme, port, query will be ignored without warning.
  return true;
}

}  // namespace enterprise_reporting

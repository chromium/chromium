// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/homepage_location_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

namespace policy {

namespace {

// Calls url_formatter::FixupURL.
GURL FixUrl(const std::string& url_spec) {
  return url_formatter::FixupURL(url_spec, std::string());
}

}  // namespace

HomepageLocationPolicyHandler::HomepageLocationPolicyHandler()
    : TypeCheckingPolicyHandler(key::kHomepageLocation,
                                base::Value::Type::STRING) {}

HomepageLocationPolicyHandler::~HomepageLocationPolicyHandler() = default;

bool HomepageLocationPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;
  if (!value)
    return true;

  // Check whether the URL is a standard scheme to prevent e.g. Javascript,
  // doing a best effort fixing invalid URLs like "example.com".
  GURL homepage_url = FixUrl(value->GetString());
  if (!homepage_url.is_valid() || !homepage_url.IsStandard()) {
    errors->AddError(policy_name(), IDS_POLICY_HOMEPAGE_LOCATION_ERROR);
    return false;
  }

  return true;
}

void HomepageLocationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* policy_value = nullptr;
  if (CheckAndGetValue(policies, nullptr, &policy_value) && policy_value) {
    prefs->SetString(prefs::kHomePage,
                     FixUrl(policy_value->GetString()).spec());
  }
}

}  // namespace policy

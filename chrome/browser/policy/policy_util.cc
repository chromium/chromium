// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_util.h"

#include <string>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/invalidation/invalidation_features.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace policy {

bool IsOriginInAllowlist(const GURL& url,
                         const PrefService* prefs,
                         const char* allowlist_pref_name,
                         const char* always_allow_pref_name) {
  DCHECK(prefs);

  if (always_allow_pref_name && prefs->GetBoolean(always_allow_pref_name)) {
    return true;
  }

  const base::Value::List& allowlisted_urls =
      prefs->GetList(allowlist_pref_name);

  if (allowlisted_urls.empty()) {
    return false;
  }

  for (auto const& value : allowlisted_urls) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(value.GetString());
    if (pattern == ContentSettingsPattern::Wildcard() || !pattern.IsValid()) {
      continue;
    }

    // Despite |url| being a GURL, the path is ignored when matching.
    if (pattern.Matches(url)) {
      return true;
    }
  }

  return false;
}

std::string GetInvalidationProjectNumber() {
  if (invalidation::IsInvalidationsWithDirectMessagesEnabled()) {
    return invalidation::InvalidationListener::kProjectNumberEnterprise;
  }
  return kPolicyFCMInvalidationSenderID;
}

}  // namespace policy

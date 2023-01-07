// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/local_heuristics.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/lookalikes/core/features.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/reputation/core/safety_tips_config.h"

bool ShouldTriggerSafetyTipFromLookalike(
    const GURL& url,
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    GURL* safe_url,
    LookalikeUrlMatchType* match_type) {
  std::string matched_domain;

  // If the domain and registry is empty, this is a private domain and thus
  // should never be flagged as malicious.
  if (navigated_domain.domain_and_registry.empty()) {
    return false;
  }

  auto* config = reputation::GetSafetyTipsRemoteConfigProto();
  const LookalikeTargetAllowlistChecker in_target_allowlist =
      base::BindRepeating(
          &reputation::IsTargetHostAllowlistedBySafetyTipsComponent, config);
  if (!GetMatchingDomain(navigated_domain, engaged_sites, in_target_allowlist,
                         config, &matched_domain, match_type)) {
    return false;
  }

  if (GetActionForMatchType(
          config, chrome::GetChannel(), navigated_domain.domain_and_registry,
          *match_type) == LookalikeActionType::kShowSafetyTip) {
    // Use https: scheme for top domain matches. Otherwise, use the lookalike
    // URL's scheme.
    // TODO(crbug.com/1190309): If the match is against an engaged site, this
    // should use the scheme of the engaged site instead.
    const std::string scheme =
        (*match_type == LookalikeUrlMatchType::kEditDistance ||
         *match_type == LookalikeUrlMatchType::kSkeletonMatchTop500 ||
         *match_type == LookalikeUrlMatchType::kSkeletonMatchTop5k)
            ? url::kHttpsScheme
            : url.scheme();
    *safe_url = GURL(scheme + url::kStandardSchemeSeparator + matched_domain);
    return true;
  }
  return false;
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/local_heuristics.h"

#include "base/bind.h"
#include "base/callback.h"
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
    GURL* safe_url) {
  std::string matched_domain;
  LookalikeUrlMatchType match_type;

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
                         config, &matched_domain, &match_type)) {
    return false;
  }

  // If we're already displaying an interstitial, don't warn again.
  if (ShouldBlockLookalikeUrlNavigation(match_type)) {
    return false;
  }

  // Use https: scheme for top domain matches. Otherwise, use the lookalike
  // URL's scheme.
  // TODO(crbug.com/1190309): If the match is against an engaged site, this
  // should use the scheme of the engaged site instead.
  const std::string scheme =
      (match_type == LookalikeUrlMatchType::kEditDistance ||
       match_type == LookalikeUrlMatchType::kSkeletonMatchTop500 ||
       match_type == LookalikeUrlMatchType::kSkeletonMatchTop5k)
          ? url::kHttpsScheme
          : url.scheme();
  *safe_url = GURL(scheme + url::kStandardSchemeSeparator + matched_domain);

  switch (match_type) {
    case LookalikeUrlMatchType::kEditDistance:
      return false;
    case LookalikeUrlMatchType::kEditDistanceSiteEngagement:
      return true;
    case LookalikeUrlMatchType::kTargetEmbedding:
      // Target Embedding should block URL Navigation.
      return false;
    case LookalikeUrlMatchType::kTargetEmbeddingForSafetyTips:
      return true;
    case LookalikeUrlMatchType::kSkeletonMatchTop5k:
      return true;
    case LookalikeUrlMatchType::kFailedSpoofChecks:
      // For now, no safety tip is shown for domain names that fail spoof checks
      // and don't have a suggested URL.
      return false;
    case LookalikeUrlMatchType::kSkeletonMatchSiteEngagement:
    case LookalikeUrlMatchType::kSkeletonMatchTop500:
      // We should only ever reach these cases when the lookalike interstitial
      // is disabled. Now that interstitial is fully launched, this only happens
      // in tests.
      NOTREACHED();
      return false;
    case LookalikeUrlMatchType::kCharacterSwapSiteEngagement:
    case LookalikeUrlMatchType::kCharacterSwapTop500:
      return true;
    case LookalikeUrlMatchType::kComboSquatting:
      return IsHeuristicEnabledForHostname(
          config,
          reputation::HeuristicLaunchConfig::
              HEURISTIC_COMBO_SQUATTING_TOP_DOMAINS,
          navigated_domain.domain_and_registry, chrome::GetChannel());
    case LookalikeUrlMatchType::kComboSquattingSiteEngagement:
      return IsHeuristicEnabledForHostname(
          config,
          reputation::HeuristicLaunchConfig::
              HEURISTIC_COMBO_SQUATTING_ENGAGED_SITES,
          navigated_domain.domain_and_registry, chrome::GetChannel());
    case LookalikeUrlMatchType::kNone:
      NOTREACHED();
  }

  NOTREACHED();
  return false;
}

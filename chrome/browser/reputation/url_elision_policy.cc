// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/url_elision_policy.h"

#include "base/feature_list.h"
#include "chrome/browser/reputation/local_heuristics.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/reputation/core/safety_tips_config.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"

#include "url/gurl.h"

namespace {

const base::FeatureParam<int> kMaximumUnelidedHostnameLength{
    &omnibox::kMaybeElideToRegistrableDomain, "max_unelided_host_length", 25};
const base::FeatureParam<bool> kEnableKeywordBasedElision{
    &omnibox::kMaybeElideToRegistrableDomain, "enable_keyword_elision", true};
const base::FeatureParam<bool> kSearchE2LDForKeywords{
    &omnibox::kMaybeElideToRegistrableDomain, "search_e2ld_for_keywords", true};

}  // namespace

bool ShouldElideToRegistrableDomain(const GURL& url) {
  DCHECK(base::FeatureList::IsEnabled(omnibox::kMaybeElideToRegistrableDomain));
  if (url.HostIsIPAddress()) {
    return false;
  }

  auto* proto = reputation::GetSafetyTipsRemoteConfigProto();
  if (!proto || reputation::IsUrlAllowlistedBySafetyTipsComponent(proto, url)) {
    // Not having a proto happens when the component hasn't downloaded yet. This
    // should only happen for a short window following initial Chrome install.
    return false;
  }

  auto host = url.host();
  if (static_cast<int>(host.length()) > kMaximumUnelidedHostnameLength.Get()) {
    return true;
  }

  // Hostnames using sensitive keywords (typically, brandnames) are often social
  // engineering, and thus should only show the registrable domain.
  auto eTLD_plus_one = GetETLDPlusOne(host);
  if (kEnableKeywordBasedElision.Get() &&
      HostnameContainsKeyword(url, eTLD_plus_one, top500_domains::kTopKeywords,
                              top500_domains::kNumTopKeywords,
                              kSearchE2LDForKeywords.Get())) {
    return true;
  }

  return false;
}

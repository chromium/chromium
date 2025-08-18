// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"

#include <memory>

#include "base/supports_user_data.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"

namespace enterprise_connectors {

namespace {

const void* const kReferrerChainDataKey = &kReferrerChainDataKey;

class ReferrerChainData : public base::SupportsUserData::Data {
 public:
  explicit ReferrerChainData(safe_browsing::ReferrerChain referrer_chain)
      : referrer_chain_(std::move(referrer_chain)) {}
  ~ReferrerChainData() override = default;

  const safe_browsing::ReferrerChain& referrer_chain() {
    return referrer_chain_;
  }

 private:
  safe_browsing::ReferrerChain referrer_chain_;
};

safe_browsing::ReferrerChain GetSafeBrowsingReferrerChain(
    const GURL& url,
    content::WebContents& web_contents) {
  safe_browsing::ReferrerChain referrers;
  safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
      GetForBrowserContext(web_contents.GetBrowserContext())
          ->IdentifyReferrerChainByEventURL(
              url, sessions::SessionTabHelper::IdForTab(&web_contents),
              kReferrerUserGestureLimit, &referrers);
  return referrers;
}

}  // namespace

safe_browsing::ReferrerChain GetReferrerChain(
    const GURL& url,
    content::WebContents& web_contents) {
  auto* referrer_chain_data = static_cast<ReferrerChainData*>(
      web_contents.GetUserData(kReferrerChainDataKey));
  if (referrer_chain_data) {
    return referrer_chain_data->referrer_chain();
  }

  // Try again after attempting to set the cache.
  SetReferrerChain(url, web_contents);
  referrer_chain_data = static_cast<ReferrerChainData*>(
      web_contents.GetUserData(kReferrerChainDataKey));
  if (referrer_chain_data) {
    return referrer_chain_data->referrer_chain();
  }

  return safe_browsing::ReferrerChain();
}

void SetReferrerChain(const GURL& url, content::WebContents& web_contents) {
  safe_browsing::ReferrerChain referrers =
      GetSafeBrowsingReferrerChain(url, web_contents);

  if (!referrers.empty()) {
    web_contents.SetUserData(kReferrerChainDataKey,
                             std::make_unique<ReferrerChainData>(referrers));
  }
}

}  // namespace enterprise_connectors

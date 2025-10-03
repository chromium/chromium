// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"

#include <memory>

#include "base/supports_user_data.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace enterprise_connectors {

namespace {

const void* const kReferrerChainDataKey = &kReferrerChainDataKey;

class ReferrerChainData : public base::SupportsUserData::Data {
 public:
  ReferrerChainData(GURL url, safe_browsing::ReferrerChain referrer_chain)
      : url_(std::move(url)), referrer_chain_(std::move(referrer_chain)) {}
  ~ReferrerChainData() override = default;

  const GURL& url() { return url_; }

  const safe_browsing::ReferrerChain& referrer_chain() {
    return referrer_chain_;
  }

 private:
  GURL url_;
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

safe_browsing::ReferrerChain GetOrCreateReferrerChain(
    download::DownloadItem& download_item) {
  safe_browsing::ReferrerChainEntry download_chain_entry;
  download_chain_entry.set_url(download_item.GetURL().spec());
  download_chain_entry.set_type(safe_browsing::ReferrerChainEntry::EVENT_URL);
  download_chain_entry.add_ip_addresses(download_item.GetURL().host());

  content::RenderFrameHost* rfh =
      content::DownloadItemUtils::GetRenderFrameHost(&download_item);
  if (rfh) {
    rfh = rfh->GetOutermostMainFrame();
    download_chain_entry.set_referrer_url(rfh->GetLastCommittedURL().spec());
  }
  for (const GURL& url : download_item.GetUrlChain()) {
    download_chain_entry.add_server_redirect_chain()->set_url(url.spec());
  }

  safe_browsing::ReferrerChain referrer_chain;
  referrer_chain.Add(std::move(download_chain_entry));

  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(&download_item);
  if (web_contents) {
    for (auto& entry :
         GetReferrerChain(download_item.GetTabUrl(), *web_contents)) {
      // Avoid duplicating entries in the list when the tab URL matches the
      // download URL.
      if (entry.url() != referrer_chain.at(referrer_chain.size() - 1).url()) {
        referrer_chain.Add(std::move(entry));
      }
    }
  }

  return referrer_chain;
}

safe_browsing::ReferrerChain GetCachedReferrerChain(
    download::DownloadItem& download_item) {
  auto* referrer_chain_data = static_cast<ReferrerChainData*>(
      download_item.GetUserData(kReferrerChainDataKey));
  if (referrer_chain_data) {
    return referrer_chain_data->referrer_chain();
  }
  return safe_browsing::ReferrerChain();
}

void SetReferrerChain(const GURL& url, content::WebContents& web_contents) {
  safe_browsing::ReferrerChain referrers =
      GetSafeBrowsingReferrerChain(url, web_contents);
  if (!referrers.empty()) {
    auto* referrer_chain_data = static_cast<ReferrerChainData*>(
        web_contents.GetUserData(kReferrerChainDataKey));

    // Don't cache new data if there is already a chain set for this exact URL.
    // This avoid clearing the cache when the page is refreshed.
    if (!referrer_chain_data || referrer_chain_data->url() != url) {
      web_contents.SetUserData(
          kReferrerChainDataKey,
          std::make_unique<ReferrerChainData>(url, referrers));
    }
  }
}

void SetReferrerChain(safe_browsing::ReferrerChain referrer_chain,
                      download::DownloadItem& download_item) {
  download_item.SetUserData(
      kReferrerChainDataKey,
      std::make_unique<ReferrerChainData>(download_item.GetTabUrl(),
                                          std::move(referrer_chain)));
}

bool HasCachedChainForTesting(base::SupportsUserData& supports_user_data) {
  return supports_user_data.GetUserData(kReferrerChainDataKey);
}

}  // namespace enterprise_connectors

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/dips/cookie_access_filter.h"
#include "chrome/browser/dips/cookie_mode.h"
#include "chrome/browser/dips/dips_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using content::NavigationHandle;

namespace {

// BounceDetectionState gets attached to NavigationHandle (which is a
// SupportsUserData subclass) to store data needed to detect stateful server
// redirects.
class BounceDetectionState : public base::SupportsUserData::Data {
 public:
  // The WebContents' previously committed URL at the time the navigation
  // started. Needed in case a parallel navigation commits.
  GURL initial_url;
  CookieAccessFilter filter;
};

const char kBounceDetectionStateKey[] = "BounceDetectionState";

std::string GetSite(const GURL& url) {
  const auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return domain.empty() ? url.host() : domain;
}

RedirectCategory ClassifyRedirect(CookieAccessType access, double engagement) {
  switch (access) {
    case CookieAccessType::kNone:
      return engagement > 0 ? RedirectCategory::kNoCookies_HasEngagement
                            : RedirectCategory::kNoCookies_NoEngagement;
    case CookieAccessType::kRead:
      return engagement > 0 ? RedirectCategory::kReadCookies_HasEngagement
                            : RedirectCategory::kReadCookies_NoEngagement;
    case CookieAccessType::kWrite:
      return engagement > 0 ? RedirectCategory::kWriteCookies_HasEngagement
                            : RedirectCategory::kWriteCookies_NoEngagement;
    case CookieAccessType::kReadWrite:
      return engagement > 0 ? RedirectCategory::kReadWriteCookies_HasEngagement
                            : RedirectCategory::kReadWriteCookies_NoEngagement;
  }
}

inline void UmaHistogramBounceCategory(RedirectCategory category,
                                       DIPSCookieMode mode) {
  const std::string histogram_name =
      base::StrCat({"Privacy.DIPS.BounceCategory", GetHistogramSuffix(mode)});
  base::UmaHistogramEnumeration(histogram_name, category);
}

}  // namespace

DIPSBounceDetector::DIPSBounceDetector(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DIPSBounceDetector>(*web_contents),
      dips_service_(DIPSService::Get(web_contents->GetBrowserContext())),
      site_engagement_service_(site_engagement::SiteEngagementService::Get(
          web_contents->GetBrowserContext())),
      // It's safe to use unretained because the callback is owned by this.
      stateful_server_redirect_handler_(
          base::BindRepeating(&DIPSBounceDetector::HandleStatefulServerRedirect,
                              base::Unretained(this))),
      // It's safe to use unretained because the callback is owned by this.
      stateful_redirect_handler_(
          base::BindRepeating(&DIPSBounceDetector::HandleStatefulRedirect,
                              base::Unretained(this))) {}

DIPSBounceDetector::~DIPSBounceDetector() = default;

DIPSCookieMode DIPSBounceDetector::GetCookieMode() const {
  return GetDIPSCookieMode(
      web_contents()->GetBrowserContext()->IsOffTheRecord(),
      dips_service_->ShouldBlockThirdPartyCookies());
}

void DIPSBounceDetector::HandleStatefulRedirect(const GURL& prev_url,
                                                const GURL& url,
                                                const GURL& next_url,
                                                CookieAccessType access) {
  const std::string site = GetSite(url);
  // TODO(rtarpine): all the calls to HandleStatefulRedirect() for a redirect
  // chain call GetSite() on the same prev_url and next_url. Be more efficient.
  if (site == GetSite(prev_url) || site == GetSite(next_url)) {
    // Ignore same-site redirects.
    return;
  }

  double score = site_engagement_service_->GetScore(url);
  RedirectCategory category = ClassifyRedirect(access, score);
  UmaHistogramBounceCategory(category, GetCookieMode());
}

void DIPSBounceDetector::HandleStatefulServerRedirect(
    const GURL& prev_url,
    content::NavigationHandle* navigation_handle,
    int redirect_index,
    CookieAccessType access) {
  const auto& redirect_chain = navigation_handle->GetRedirectChain();
  const GURL& url = redirect_chain[redirect_index];
  // We are called from DidFinishNavigation() so GetURL() returns the final URL.
  // XXX For 204 No Content responses, should we actually use `prev_url`, since
  // it's what the user actually sees?
  const GURL& next_url = navigation_handle->GetURL();
  stateful_redirect_handler_.Run(prev_url, url, next_url, access);
}

void DIPSBounceDetector::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  auto state = std::make_unique<BounceDetectionState>();
  state->initial_url = web_contents()->GetLastCommittedURL();
  navigation_handle->SetUserData(kBounceDetectionStateKey, std::move(state));
}

void DIPSBounceDetector::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  auto* state = static_cast<BounceDetectionState*>(
      navigation_handle->GetUserData(kBounceDetectionStateKey));
  if (state) {
    state->filter.AddAccess(details.url, details.type);
  }
}

void DIPSBounceDetector::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // We can be sure OnCookiesAccessed() was called for all redirects at this
  // point.
  auto* state = static_cast<BounceDetectionState*>(
      navigation_handle->GetUserData(kBounceDetectionStateKey));
  if (state && !state->filter.is_empty()) {
    std::vector<CookieAccessType> access_types;
    if (!state->filter.Filter(navigation_handle->GetRedirectChain(),
                              &access_types)) {
      // We failed to map all the OnCookiesAccessed calls to the redirect chain.
      // TODO(rtarpine): report metrics to see if this happens in practice
      DCHECK(false) << "CookieAccessFilter failed to map all accesses";
      return;
    }

    for (size_t i = 0; i < access_types.size() - 1; i++) {
      stateful_server_redirect_handler_.Run(
          state->initial_url, navigation_handle, i, access_types[i]);
    }
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSBounceDetector);

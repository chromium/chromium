// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/oauth_login_detector.h"

#include "chrome/browser/login_detection/login_detection_util.h"
#include "net/base/url_util.h"

namespace login_detection {

namespace {

// Returns whether all the given query parameters are found in the URL.
bool DoAllQueryParamsExist(const std::set<std::string>& request_params,
                           const GURL& url) {
  if (!url.has_query())
    return false;
  for (const auto& param : request_params) {
    std::string param_value;
    if (!net::GetValueForKeyInQuery(url, param, &param_value))
      return false;
  }
  return true;
}

}  // namespace

OAuthLoginDetector::OAuthLoginFlowInfo::OAuthLoginFlowInfo(
    const GURL& oauth_provider_site,
    const GURL& oauth_requestor_site)
    : oauth_provider_site(oauth_provider_site),
      oauth_requestor_site(oauth_requestor_site) {}

OAuthLoginDetector::OAuthLoginFlowInfo::OAuthLoginFlowInfo(
    const OAuthLoginFlowInfo&) = default;

OAuthLoginDetector::OAuthLoginFlowInfo::~OAuthLoginFlowInfo() = default;

OAuthLoginDetector::OAuthLoginDetector()
    : login_flow_start_query_params_(GetOAuthLoginStartQueryParams()),
      login_flow_complete_query_params_(GetOAuthLoginCompleteQueryParams()) {
  DCHECK(IsLoginDetectionFeatureEnabled());
}

OAuthLoginDetector::~OAuthLoginDetector() = default;

std::optional<GURL> OAuthLoginDetector::GetSuccessfulLoginFlowSite(
    const GURL& prev_navigation_url,
    const std::vector<GURL>& redirect_chain) {
  for (size_t i = 0; i < redirect_chain.size(); i++) {
    GURL navigation_url = redirect_chain[i];
    // Allow login flows to be detected only on HTTPS pages.
    if (!navigation_url.SchemeIs(url::kHttpsScheme)) {
      login_flow_info_.reset();
      return std::nullopt;
    }

    // Check for OAuth login completion.
    if (login_flow_info_ && CheckSuccessfulLoginCompletion(navigation_url)) {
      auto oauth_requestor_site = login_flow_info_->oauth_requestor_site;
      login_flow_info_.reset();
      return oauth_requestor_site;
    }

    // Check for start of login flow.
    if (!login_flow_info_ &&
        DoAllQueryParamsExist(login_flow_start_query_params_, navigation_url)) {
      if (popup_opener_navigation_site_) {
        // When this detector is opened for a popup window, treat the site of
        // the popup opener window site as the OAuth requestor site.
        login_flow_info_ =
            OAuthLoginFlowInfo(navigation_url, *popup_opener_navigation_site_);
      } else if (prev_navigation_url.is_valid() &&
                 prev_navigation_url.SchemeIsHTTPOrHTTPS()) {
        login_flow_info_ = OAuthLoginFlowInfo(
            navigation_url, prev_navigation_url.DeprecatedGetOriginAsURL());
      } else if (i != 0) {
        // Treat the start of the redirect chain as the previous navigation URL.
        // This allows detecting cases when a new window is opened to perform
        // the OAuth login.
        login_flow_info_ = OAuthLoginFlowInfo(
            navigation_url, redirect_chain[0].DeprecatedGetOriginAsURL());
      }
    }
  }
  if (login_flow_info_)
    login_flow_info_->count_navigations_since_login_flow_start++;
  return std::nullopt;
}

void OAuthLoginDetector::DidOpenAsPopUp(const GURL& opener_navigation_url) {
  if (opener_navigation_url.is_valid() &&
      opener_navigation_url.SchemeIs(url::kHttpsScheme)) {
    popup_opener_navigation_site_ =
        opener_navigation_url.DeprecatedGetOriginAsURL();
  }
}

std::optional<GURL> OAuthLoginDetector::GetPopUpLoginFlowSite() const {
  // OAuth has never started.
  if (!login_flow_info_)
    return std::nullopt;

  // Only consider OAuth completion when this is a popup window.
  if (!popup_opener_navigation_site_)
    return std::nullopt;

  return login_flow_info_->oauth_requestor_site;
}

bool OAuthLoginDetector::CheckSuccessfulLoginCompletion(
    const GURL& navigation_url) {
  DCHECK(login_flow_info_.has_value());

  // Login flow had started previously, check if it completes within the
  // navigation limit.
  if (login_flow_info_->count_navigations_since_login_flow_start >
      GetOAuthLoginFlowStartToCompleteLimit()) {
    // Navigation limit reached - reset the state so that login flow was never
    // started.
    login_flow_info_.reset();
    return false;
  }

  // Check the OAuth login completion that returns the authorzation code and
  // token to the OAuth requestor site, does not happen for the OAuth provider
  // site.
  if (GetSiteNameForURL(navigation_url) ==
      GetSiteNameForURL(login_flow_info_->oauth_provider_site)) {
    return false;
  }

  // Check for OAuth login completion parameters. This should not happen for the
  // OAuth provider site, since this returns the authorzation code and token to
  // the OAuth requestor site.
  if (DoAllQueryParamsExist(login_flow_complete_query_params_,
                            navigation_url)) {
    return true;
  }

  // PopUp based login completion flow should only navigate within the OAuth
  // provider site.
  if (popup_opener_navigation_site_)
    login_flow_info_.reset();

  return false;
}

}  // namespace login_detection

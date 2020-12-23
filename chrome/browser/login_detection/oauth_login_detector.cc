// Copyright 2020 The Chromium Authors. All rights reserved.
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

base::Optional<GURL> OAuthLoginDetector::GetSuccessfulLoginFlowSite(
    const GURL& prev_navigation_url,
    const std::vector<GURL>& redirect_chain) {
  for (const auto& navigation_url : redirect_chain) {
    // Allow login flows to be detected only on HTTPS pages.
    if (!navigation_url.SchemeIs(url::kHttpsScheme)) {
      login_flow_info_ = base::nullopt;
      return base::nullopt;
    }

    // Check for OAuth login completion.
    if (login_flow_info_ && CheckSuccessfulLoginCompletion(navigation_url)) {
      auto oauth_requestor_site = login_flow_info_->oauth_requestor_site;
      login_flow_info_ = base::nullopt;
      return oauth_requestor_site;
    }

    // Check for start of login flow.
    if (!login_flow_info_ && prev_navigation_url.is_valid() &&
        prev_navigation_url.SchemeIsHTTPOrHTTPS() &&
        DoAllQueryParamsExist(login_flow_start_query_params_, navigation_url)) {
      login_flow_info_ =
          OAuthLoginFlowInfo(navigation_url, prev_navigation_url.GetOrigin());
    }
  }
  return base::nullopt;
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
    login_flow_info_ = base::nullopt;
    return false;
  }
  std::string navigation_site = GetSiteNameForURL(navigation_url);

  // Check the OAuth login completion that returns the authorzation code and
  // token to the OAuth requestor site, does not happen for the OAuth provider
  // site.
  if (GetSiteNameForURL(login_flow_info_->oauth_provider_site) ==
      navigation_site) {
    login_flow_info_->count_navigations_since_login_flow_start++;
    return false;
  }

  if (!DoAllQueryParamsExist(login_flow_complete_query_params_,
                             navigation_url)) {
    login_flow_info_->count_navigations_since_login_flow_start++;
    return false;
  }
  return true;
}

}  // namespace login_detection

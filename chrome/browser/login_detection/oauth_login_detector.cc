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
    const std::string& start_site)
    : start_site(start_site) {}

OAuthLoginDetector::OAuthLoginFlowInfo::OAuthLoginFlowInfo(
    const OAuthLoginFlowInfo&) = default;

OAuthLoginDetector::OAuthLoginFlowInfo::~OAuthLoginFlowInfo() = default;

OAuthLoginDetector::OAuthLoginDetector()
    : login_flow_start_query_params_(GetOAuthLoginStartQueryParams()),
      login_flow_complete_query_params_(GetOAuthLoginCompleteQueryParams()) {
  DCHECK(IsLoginDetectionFeatureEnabled());
}

OAuthLoginDetector::~OAuthLoginDetector() = default;

bool OAuthLoginDetector::CheckSuccessfulLoginFlow(const GURL& navigation_url) {
  // Allow login flows to be detected only on HTTPS pages.
  if (!navigation_url.SchemeIs(url::kHttpsScheme)) {
    login_flow_info_ = base::nullopt;
    return false;
  }

  if (!login_flow_info_) {
    if (DoAllQueryParamsExist(login_flow_start_query_params_, navigation_url)) {
      // Login flow start was detected.
      login_flow_info_ = OAuthLoginFlowInfo(GetSiteNameForURL(navigation_url));
    }
    return false;
  }

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

  // Check the navigation only happens for the start site or completion site.
  if (login_flow_info_->start_site != navigation_site &&
      login_flow_info_->completion_site &&
      *login_flow_info_->completion_site != navigation_site) {
    login_flow_info_ = base::nullopt;
    return false;
  }
  // Update any navigation to a non start site as completion site.
  if (login_flow_info_->start_site != navigation_site &&
      !login_flow_info_->completion_site) {
    login_flow_info_->completion_site = navigation_site;
  }

  DCHECK(login_flow_info_->start_site == navigation_site ||
         *login_flow_info_->completion_site == navigation_site);
  if (!DoAllQueryParamsExist(login_flow_complete_query_params_,
                             navigation_url)) {
    login_flow_info_->count_navigations_since_login_flow_start++;
    return false;
  }
  // Successful login flow completion was detected.
  login_flow_info_ = base::nullopt;
  return true;
}

}  // namespace login_detection

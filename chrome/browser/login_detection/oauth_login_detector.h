// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_OAUTH_LOGIN_DETECTOR_H_
#define CHROME_BROWSER_LOGIN_DETECTION_OAUTH_LOGIN_DETECTOR_H_

#include <set>
#include <string>

#include "base/optional.h"
#include "url/gurl.h"

namespace login_detection {

// Detects successful OAuth login flow based on heuristics that observe certain
// request parameters to determine start and completion of OAuth login flow.
//
// Initially, there is a navigation to the OAuth requestor site, and that
// triggers navigation to the OAuth provider site with certain request
// parameters to identify the OAuth start.
//
// The OAuth requestor authenticates the user and returns the authorization code
// or token to the requestor. This redirect request has certain reqest
// parameters to identify as OAuth completion. Note that this requestor site can
// be quite different from the OAuth requestor site seen in the initial
// navigation.
class OAuthLoginDetector {
 public:
  OAuthLoginDetector();
  ~OAuthLoginDetector();

  OAuthLoginDetector(const OAuthLoginDetector&) = delete;
  OAuthLoginDetector& operator=(const OAuthLoginDetector&) = delete;

  // Processes the navigation |redirect_chain| and returns the site that started
  // the OAuth login flow and completed. base::nullopt is returned when there is
  // no login flow detected or it has not yet completed. |prev_navigation_url|
  // is the URL of the previous navigation on this detector, and can be invalid
  // when no previous navigation happened.
  base::Optional<GURL> GetSuccessfulLoginFlowSite(
      const GURL& prev_navigation_url,
      const std::vector<GURL>& redirect_chain);

 private:
  struct OAuthLoginFlowInfo {
    OAuthLoginFlowInfo(const GURL& oauth_provider_site,
                       const GURL& oauth_requestor_site);
    OAuthLoginFlowInfo(const OAuthLoginFlowInfo&);
    ~OAuthLoginFlowInfo();

    // Number of navigations happened since the start of OAuth login flow was
    // detected. The default value starts from 1, as the start navigation is
    // counted as well.
    size_t count_navigations_since_login_flow_start = 1;

    // The OAuth provider site.
    GURL oauth_provider_site;

    // The the OAuth requestor site that initiated the login flow.
    GURL oauth_requestor_site;
  };

  // Returns whether successful OAuth login completion was detected. Clears the
  // login flow state when completion is detected or when completion is no
  // longer possible.
  bool CheckSuccessfulLoginCompletion(const GURL& navigation_url);

  // Set of query parameters that should be found in the navigation URL to
  // recognize the navigation as a start and completion of OAuth login flow.
  // These are populated from field trail.
  const std::set<std::string> login_flow_start_query_params_;
  const std::set<std::string> login_flow_complete_query_params_;

  // Info about the current login flow. Exists only when there is an ongoing
  // OAuth login flow. Created on the start of login flow, and destroyed when
  // the flow completes successfully or navigation limit is reached.
  base::Optional<OAuthLoginFlowInfo> login_flow_info_;
};

}  // namespace login_detection

#endif  // CHROME_BROWSER_LOGIN_DETECTION_OAUTH_LOGIN_DETECTOR_H_

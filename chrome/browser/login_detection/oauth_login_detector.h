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

// Detects successful OAuth login flow based on heuristics.
class OAuthLoginDetector {
 public:
  OAuthLoginDetector();
  ~OAuthLoginDetector();

  OAuthLoginDetector(const OAuthLoginDetector&) = delete;
  OAuthLoginDetector& operator=(const OAuthLoginDetector&) = delete;

  // Processes the navigation to |navigation_url| and returns whether a
  // successful OAuth login flow started and completed.
  bool CheckSuccessfulLoginFlow(const GURL& navigation_url);

 private:
  struct OAuthLoginFlowInfo {
    explicit OAuthLoginFlowInfo(const std::string& start_site);
    OAuthLoginFlowInfo(const OAuthLoginFlowInfo&);
    ~OAuthLoginFlowInfo();

    // Number of navigations happened since the start of OAuth login flow was
    // detected. The default value starts from 1, as the start navigation is
    // counted as well.
    size_t count_navigations_since_login_flow_start = 1;

    // Site that started the login flow. This is the OAuth provider site.
    std::string start_site;

    // Site that completed the login flow. This is the OAuth requestor site.
    // Empty when OAuth flow started until a completion site is detected.
    base::Optional<std::string> completion_site;
  };

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

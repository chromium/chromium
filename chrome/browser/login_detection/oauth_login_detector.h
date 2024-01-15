// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_OAUTH_LOGIN_DETECTOR_H_
#define CHROME_BROWSER_LOGIN_DETECTION_OAUTH_LOGIN_DETECTOR_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "url/gurl.h"

namespace login_detection {

// Detects successful OAuth login flow based on heuristics that observe certain
// request parameters to determine start and completion of OAuth login flow.
//
// OAuth Start:
// Initially, there is a navigation to the OAuth requestor site, and that
// triggers navigation to the OAuth provider site with certain request
// parameters to identify the OAuth start. This start navigation could happen on
// the same page as the initial navigation or on a new popup window using
// window.open(), in which case DidOpenAsPopUp() is used to keep track of the
// requestor site.
//
// OAuth Completion:
// The OAuth provider authenticates the user and returns the authorization code
// or token to the requestor. This redirect request has certain reqest
// parameters to identify as OAuth completion. Note that this requestor site can
// be quite different from the OAuth requestor site seen in the initial
// navigation.
// OAuth completion is different in the case of popup based login flow. The
// authorization code is sent via different means (for example postMessage()),
// and is not easily detectable. So, closing of the popup can be used as OAuth
// completion signal. These detections could be false positives, but are reduced
// by allowing only navigations to the OAuth provider site, and by limiting the
// number of navigations in the popup window.
class OAuthLoginDetector {
 public:
  OAuthLoginDetector();
  ~OAuthLoginDetector();

  OAuthLoginDetector(const OAuthLoginDetector&) = delete;
  OAuthLoginDetector& operator=(const OAuthLoginDetector&) = delete;

  // Processes the navigation |redirect_chain| and returns the site that started
  // the OAuth login flow and completed. std::nullopt is returned when there is
  // no login flow detected or it has not yet completed. |prev_navigation_url|
  // is the URL of the previous navigation on this detector, and can be invalid
  // when no previous navigation happened.
  std::optional<GURL> GetSuccessfulLoginFlowSite(
      const GURL& prev_navigation_url,
      const std::vector<GURL>& redirect_chain);

  // Returns the OAuth requestor site when popup based login flow is detected,
  // otherwise std::nullopt is returned.
  std::optional<GURL> GetPopUpLoginFlowSite() const;

  // Indicates this detector is opened for a popup window, and the opener window
  // had the |opener_navigation_url|.
  void DidOpenAsPopUp(const GURL& opener_navigation_url);

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
  std::optional<OAuthLoginFlowInfo> login_flow_info_;

  // The site that opened this detector window as a popup. std::nullopt when
  // this detector is not opened as a popup.
  std::optional<GURL> popup_opener_navigation_site_;
};

}  // namespace login_detection

#endif  // CHROME_BROWSER_LOGIN_DETECTION_OAUTH_LOGIN_DETECTOR_H_

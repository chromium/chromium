// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_UTIL_H_
#define CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_UTIL_H_

#include <set>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "url/gurl.h"

namespace login_detection {

// Enables login detection to sited based on various heuristics.
BASE_DECLARE_FEATURE(kLoginDetection);

// Returns whether login detection should be enabled.
bool IsLoginDetectionFeatureEnabled();

// Returns the site which is the scheme and effective TLD+1 of the URL. The
// other components of the URL such as the port and path are ignored.
std::string GetSiteNameForURL(const GURL& url);

// Returns the query parameters that should be found in the navigation URL to
// recognize that as start of OAuth login flow.
std::set<std::string> GetOAuthLoginStartQueryParams();

// Returns the query parameters that should be found in the navigation URL to
// recognize that as successful completion of OAuth login flow.
std::set<std::string> GetOAuthLoginCompleteQueryParams();

// Returns the number of navigations within which OAuth login flow should start
// and complete successfully.
size_t GetOAuthLoginFlowStartToCompleteLimit();

// Returns the maximum allowed number of OAuth logged-in sites. This is to cap
// the size of the pref that stores the list.
size_t GetOauthLoggedInSitesMaxSize();

// Returns the sites retrieved from field trial, that should be treated as
// logged-in.
std::vector<std::string> GetLoggedInSitesFromFieldTrial();

}  // namespace login_detection

#endif  // CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_UTIL_H_

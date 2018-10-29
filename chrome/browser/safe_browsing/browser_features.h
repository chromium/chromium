// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Client-side phishing features that are extracted by the browser, after
// receiving a score from the renderer.

#ifndef CHROME_BROWSER_SAFE_BROWSING_BROWSER_FEATURES_H_
#define CHROME_BROWSER_SAFE_BROWSING_BROWSER_FEATURES_H_

namespace safe_browsing {

// IMPORTANT: when adding new features, you must update kAllowedFeatures in
// chrome/browser/safe_browsing/client_side_detection_service.cc if the feature
// should be sent in sanitized pingbacks.
//
////////////////////////////////////////////////////
// History features.
////////////////////////////////////////////////////

// Number of visits to that URL stored in the browser history.
// Should always be an integer larger than 1 because by the time
// we lookup the history the current URL should already be stored there.
extern const char kUrlHistoryVisitCount[];

// Number of times the URL was typed in the Omnibox.
extern const char kUrlHistoryTypedCount[];

// Number of times the URL was reached by clicking a link.
extern const char kUrlHistoryLinkCount[];

// Number of times URL was visited more than 24h ago.
extern const char kUrlHistoryVisitCountMoreThan24hAgo[];

// Number of user-visible visits to all URLs on the same host/port as
// the URL for HTTP and HTTPs.
extern const char kHttpHostVisitCount[];
extern const char kHttpsHostVisitCount[];

// Boolean feature which is true if the host was visited for the first
// time more than 24h ago (only considers user-visible visits like above).
extern const char kFirstHttpHostVisitMoreThan24hAgo[];
extern const char kFirstHttpsHostVisitMoreThan24hAgo[];

////////////////////////////////////////////////////
// Browse features.
////////////////////////////////////////////////////
// Note that these features may have the following prefixes appended to them
// that tell for which page type the feature pertains.
extern const char kHostPrefix[];

// Referrer
extern const char kReferrer[];
// True if the referrer was stripped because it is an SSL referrer.
extern const char kHasSSLReferrer[];
// Stores the page transition.  See: PageTransition.  We strip the qualifier.
extern const char kPageTransitionType[];
// True if this navigation is the first for this tab.
extern const char kIsFirstNavigation[];
// Feature that is set if the url from the navigation entry doesn't match the
// url at the end of the redirect chain.
extern const char kRedirectUrlMismatch[];
// The redirect chain that leads to the named page.
extern const char kRedirect[];
// If a redirect is SSL, we will use this value instead of the actual redirect
// so we don't leak any SSL sites.
extern const char kSecureRedirectValue[];

// The HTTP status code for the main document.
extern const char kHttpStatusCode[];

// SafeBrowsing related featues.  Fields from the UnsafeResource if there is
// any.
extern const char kSafeBrowsingMaliciousUrl[];
extern const char kSafeBrowsingOriginalUrl[];
extern const char kSafeBrowsingIsSubresource[];
extern const char kSafeBrowsingThreatType[];
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_BROWSER_FEATURES_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/browser_features.h"

namespace safe_browsing {
const char kUrlHistoryVisitCount[] = "UrlHistoryVisitCount";
const char kUrlHistoryTypedCount[] = "UrlHistoryTypedCount";
const char kUrlHistoryLinkCount[] = "UrlHistoryLinkCount";
const char kUrlHistoryVisitCountMoreThan24hAgo[] =
    "UrlHistoryVisitCountMoreThan24hAgo";
const char kHttpHostVisitCount[] = "HttpHostVisitCount";
const char kHttpsHostVisitCount[] = "HttpsHostVisitCount";
const char kFirstHttpHostVisitMoreThan24hAgo[] =
    "FirstHttpHostVisitMoreThan24hAgo";
const char kFirstHttpsHostVisitMoreThan24hAgo[] =
    "FirstHttpsHostVisitMoreThan24hAgo";

const char kHostPrefix[] = "Host";
const char kReferrer[] = "Referrer";
const char kHasSSLReferrer[] = "HasSSLReferrer";
const char kPageTransitionType[] = "PageTransitionType";
const char kIsFirstNavigation[] = "IsFirstNavigation";
const char kRedirectUrlMismatch[] = "RedirectUrlMismatch";
const char kRedirect[] = "Redirect";
const char kSecureRedirectValue[] = "SecureRedirect";
const char kHttpStatusCode[] = "HttpStatusCode";
const char kSafeBrowsingMaliciousUrl[] = "SafeBrowsingMaliciousUrl=";
const char kSafeBrowsingOriginalUrl[] = "SafeBrowsingOriginalUrl=";
const char kSafeBrowsingIsSubresource[] = "SafeBrowsingIsSubresource";
const char kSafeBrowsingThreatType[] = "SafeBrowsingThreatType";
}  // namespace safe_browsing

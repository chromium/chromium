// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_PREFS_H_
#define CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_PREFS_H_

#include "url/gurl.h"
#include "url/origin.h"

class PrefRegistrySimple;
class PrefService;

namespace login_detection {

namespace prefs {

// Registers the prefs used by login detection.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Clears the prefs.
void RemoveLoginDetectionData(PrefService* prefs);

// Save the site of the url to the list of OAuth signed-in sites. The effective
// TLD+1 of the URL is used as the site.
void SaveSiteToOAuthSignedInList(PrefService* pref_service, const GURL& url);

// Returns whether the site of the url exists in the list of OAuth signed-in
// sites. The effective TLD+1 of the URL is used as the site.
bool IsSiteInOAuthSignedInList(PrefService* pref_service, const GURL& url);

// Returns a list of url::Origins representing saved OAuth signed-in sites.
// Note that entries in this list consist of sites (i.e., scheme and eTLD+1).
std::vector<url::Origin> GetOAuthSignedInSites(PrefService* pref_service);

}  // namespace prefs

}  // namespace login_detection

#endif  // CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_PREFS_H_

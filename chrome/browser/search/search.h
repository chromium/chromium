// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_H_
#define CHROME_BROWSER_SEARCH_SEARCH_H_

#include "build/build_config.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
class NavigationEntry;
class WebContents;
}

namespace search {

// Returns whether Google is selected as the default search engine.
bool DefaultSearchProviderIsGoogle(Profile* profile);

// Returns true if |url| corresponds to a New Tab page or its service worker.
bool IsNTPOrRelatedURL(const GURL& url, Profile* profile);

// Returns whether a |url| corresponds to a New Tab page.
bool IsNTPURL(const GURL& url);

// Returns true if the active navigation entry of |contents| is a New Tab page
// rendered in an Instant process. This is the last committed entry if it
// exists, and otherwise the visible entry.
bool IsInstantNTP(content::WebContents* contents);

// Same as IsInstantNTP but uses |nav_entry| to determine the URL for the page
// instead of using the visible entry.
bool NavEntryIsInstantNTP(content::WebContents* contents,
                          content::NavigationEntry* nav_entry);

// Returns true if |url| corresponds to a New Tab page that would get rendered
// in an Instant process.
bool IsInstantNTPURL(const GURL& url, Profile* profile);

// Returns the New Tab page URL for the given |profile|.
GURL GetNewTabPageURL(Profile* profile);

#if !BUILDFLAG(IS_ANDROID)

// Returns true if |url| should be rendered in the Instant renderer process.
bool ShouldAssignURLToInstantRenderer(const GURL& url, Profile* profile);

// Returns true if the Instant |site_url| should use process per site.
bool ShouldUseProcessPerSiteForInstantSiteURL(const GURL& site_url,
                                              Profile* profile);

// Transforms the input |url| into its "effective URL". |url| must be an
// Instant URL, i.e. ShouldAssignURLToInstantRenderer must return true. The
// returned URL facilitates grouping process-per-site. The |url| is transformed,
// for example, from
//
//   https://www.google.com/search?espv=1&q=tractors
//
// to the privileged URL
//
//   chrome-search://www.google.com/search?espv=1&q=tractors
//
// Notice the scheme change.
//
// If the input is already a privileged URL then that same URL is returned.
//
// If |url| is that of the online NTP, its host is replaced with "remote-ntp".
// This forces the NTP and search results pages to have different SiteIntances,
// and hence different processes.
GURL GetEffectiveURLForInstant(const GURL& url, Profile* profile);

// Rewrites |url| to the actual NTP URL to use if
//   1. |url| is "chrome://newtab" or starts with "chrome-search://local-ntp",
//   2. InstantExtended is enabled, and
//   3. |browser_context| doesn't correspond to an incognito profile.
// chrome://new-tab-page or chrome://new-tab-page-third-party to handle
// unexplained usage.
bool HandleNewTabURLRewrite(GURL* url,
                            content::BrowserContext* browser_context);
// Reverses the operation from HandleNewTabURLRewrite.
bool HandleNewTabURLReverseRewrite(GURL* url,
                                   content::BrowserContext* browser_context);

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace search

#endif  // CHROME_BROWSER_SEARCH_SEARCH_H_

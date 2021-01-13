// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_UTIL_H_
#define CHROME_BROWSER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_UTIL_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace subresource_redirect {

class OriginRobotsRulesCache;

// Returns whether LiteMode is enabled for the profile associated with the
// |web_contents|.
bool IsLiteModeEnabled(content::WebContents* web_contents);

// Returns if the public image hints based subresource compression is enabled.
bool ShouldEnablePublicImageHintsBasedCompression();

// Returns if the login and robots checks based subresource compression is
// enabled. This compresses non logged-in pages and subresources allowed by
// robots.txt rules.
bool ShouldEnableLoginRobotsCheckedCompression();

// Should the subresource be redirected to its compressed version. This returns
// false if only coverage metrics need to be recorded and actual redirection
// should not happen.
bool ShouldCompressRedirectSubresource();

// Returns whether image compression should be applied for this web_contents.
// Also shows an one-time InfoBar on Android if needed.
bool ShowInfoBarAndGetImageCompressionState(
    content::WebContents* web_contents,
    content::NavigationHandle* navigation_handle);

// Notifies to LiteMode that image compression fetch had failed.
void NotifyCompressedImageFetchFailed(content::WebContents* web_contents,
                                      base::TimeDelta retry_after);

// Returns the LitePages robots rules server endpoint URL to fetch for the given
// |origin|.
GURL GetRobotsServerURL(const url::Origin& origin);

// Returns the robots rules cache for the profile of |web_contents|.
OriginRobotsRulesCache* GetOriginRobotsRulesCache(
    content::WebContents* web_contents);

// Returns the maximum number of origin robots rules the browser should cache
// in-memory to be sent to the renderers immediately.
int MaxOriginRobotsRulesCacheSize();

// Returns a random duration LitePages service should bypass for, when a
// LitePages response fails without RetryAfter header.
base::TimeDelta GetLitePagesBypassRandomDuration();

// Returns the maximum duration LitePages service should be bypassed.
base::TimeDelta GetLitePagesBypassMaxDuration();

}  // namespace subresource_redirect

#endif  // CHROME_BROWSER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_UTIL_H_

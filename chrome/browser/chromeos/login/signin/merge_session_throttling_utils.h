// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_MERGE_SESSION_THROTTLING_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_MERGE_SESSION_THROTTLING_UTILS_H_

class GURL;
class Profile;

namespace content {
class WebContents;
}

// A set of helper functions used by the MergeSessionNavigationThrottle and the
// RendererUpdater to determine if an interstitial page should be
// shown for a request when the merge session process (cookie reconstruction
// from OAuth2 refresh token in ChromeOS login) is still in progress.
namespace merge_session_throttling_utils {

// Policy for when it is valid to attach a MergeSessionNavigationThrottle.
// Namely, this will be false for unit tests, where the UserManager is not
// initialized.
bool ShouldAttachNavigationThrottle();

// Checks if session is already merged. This is safe to call on all threads.
bool AreAllSessionMergedAlready();

// Adds/removes |profile| to/from the blocking profiles set. This should be
// called on the UI thread.
void BlockProfile(Profile* profile);
void UnblockProfile(Profile* profile);

// Whether requests from |web_contents| or |profile| should currently be
// delayed. This should be called on the UI thread.
bool ShouldDelayRequestForProfile(Profile* profile);
bool ShouldDelayRequestForWebContents(content::WebContents* web_contents);

// True if the load of |url| should be delayed. The function is safe to be
// called on any thread.
bool ShouldDelayUrl(const GURL& url);

// True if session restore hasn't started or in progress.
bool IsSessionRestorePending(Profile* profile);

}  // namespace merge_session_throttling_utils

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_MERGE_SESSION_THROTTLING_UTILS_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_UTILS_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_UTILS_H_

class BrowserWindowInterface;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace glic {

// True if the immutable attributes of `browser` are valid for Glic focus.
// or pinning. Invalid browsers are never observed.
bool IsBrowserValidForSharingInProfile(
    BrowserWindowInterface* browser_interface,
    Profile* profile);

// Returns true if `web_contents` can be shared, given its current state.
// This becomes invalid when the committed URL changes.
// Sharing may still fail for other reasons.
bool IsTabValidForSharing(content::WebContents* web_contents);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_UTILS_H_

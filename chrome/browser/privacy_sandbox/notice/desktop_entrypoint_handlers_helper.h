// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_ENTRYPOINT_HANDLERS_HELPER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_ENTRYPOINT_HANDLERS_HELPER_H_

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace privacy_sandbox {
// Returns whether |url| is suitable to display the Privacy Sandbox prompt
// over. Only about:blank and certain chrome:// URLs are considered
// suitable.
bool IsUrlSuitableForPrompt(const GURL& url);

#if BUILDFLAG(IS_CHROMEOS)
// Opens AboutBlank on Chrome for non-Chrome controlled NTP navigations.
void MaybeOpenAboutBlankOnChrome(content::NavigationHandle* navigation_handle,
                                 Profile* profile,
                                 content::WebContents* web_contents);
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace privacy_sandbox
#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_ENTRYPOINT_HANDLERS_HELPER_H_

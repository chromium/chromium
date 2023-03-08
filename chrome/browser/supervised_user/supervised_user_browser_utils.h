
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_

#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace supervised_user {

// Returns true if both the extensions are enabled and the provived
// url is a Webstore or Download url.
bool IsSupportedChromeExtensionURL(const GURL& effective_url);

// Returns true if the parent allowlist should be skipped.
bool ShouldContentSkipParentAllowlistFiltering(content::WebContents* contents);

}  // namespace supervised_user

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_

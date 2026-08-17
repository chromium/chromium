// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SUSPICIOUS_SITE_WARNINGS_SUSPICIOUS_SITE_UI_H_
#define CHROME_BROWSER_SAFE_BROWSING_SUSPICIOUS_SITE_WARNINGS_SUSPICIOUS_SITE_UI_H_

#include <stdint.h>

namespace content {
class WebContents;
}

namespace safe_browsing {

// Shows the suspicious site warning for |web_contents| and |navigation_id|.
// Implemented by platform-specific controllers.
void ShowSuspiciousSiteWarning(content::WebContents* web_contents,
                               int64_t navigation_id);

// Shows the suspicious site warning bubble for |web_contents|. Implemented in
// platform-specific UI files.
void ShowSuspiciousSiteBubble(content::WebContents* web_contents);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SUSPICIOUS_SITE_WARNINGS_SUSPICIOUS_SITE_UI_H_

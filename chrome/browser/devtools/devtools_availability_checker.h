// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AVAILABILITY_CHECKER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AVAILABILITY_CHECKER_H_

class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace web_app {
class WebApp;
}

// |web_contents| may be null, in which case this function just checks
// the settings for |profile|.
bool IsInspectionAllowed(Profile* profile, content::WebContents* web_contents);

// |extension| may be null, in which case this function just checks
// the settings for |profile|.
bool IsInspectionAllowed(Profile* profile,
                         const extensions::Extension* extension);

// |web_app| may be null, in which case this function just checks
// the settings for |profile|.
bool IsInspectionAllowed(Profile* profile, const web_app::WebApp* web_app);

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AVAILABILITY_CHECKER_H_

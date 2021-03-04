// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client_parts.h"

namespace content {
class WebContents;
}

namespace blink {
namespace web_pref {
struct WebPreferences;
}  // namespace web_pref
}  // namespace blink

bool ChromeContentBrowserClientParts::OverrideWebPreferencesAfterNavigation(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* web_prefs) {
  return false;
}

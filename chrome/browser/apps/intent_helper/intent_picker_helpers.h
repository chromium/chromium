// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_

#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace apps {

// Displays the intent picker bubble in the omnibar if the last committed URL
// has corresponding apps that can open the page.
// Returns if the intent icon should be shown.
[[nodiscard]] bool MaybeShowIntentPicker(
    content::NavigationHandle* navigation_handle);
// Overload used to check if the intent picker can be displayed,
// only on non Chrome OS devices.
// Also used to recheck after content is reparented.
void MaybeShowIntentPicker(content::WebContents* web_contents);

void ShowIntentPickerBubble(content::WebContents* web_contents,
                            const GURL& url);

// Returns true if persistence for PWA entries in the Intent Picker is enabled.
bool IntentPickerPwaPersistenceEnabled();

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_

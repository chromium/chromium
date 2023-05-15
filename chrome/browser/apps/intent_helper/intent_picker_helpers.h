// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_

#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace apps {

// Displays the intent picker icon in the omnibox, based on the last committed
// URL in |web_contents|.
void MaybeShowIntentPicker(content::WebContents* web_contents);

// Shows the intent picker bubble to present a choice between apps to handle
// |url|. May launch directly into an app based on user preferences and
// installed apps.
void ShowIntentPickerOrLaunchApp(content::WebContents* web_contents,
                                 const GURL& url);

// Returns true if persistence for PWA entries in the Intent Picker is enabled.
bool IntentPickerPwaPersistenceEnabled();

// Returns the size, in dp, of app icons shown in the intent picker bubble.
int GetIntentPickerBubbleIconSize();

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_

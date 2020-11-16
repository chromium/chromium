// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_

#include <vector>

#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace apps {

void MaybeShowIntentPicker(content::NavigationHandle* navigation_handle);

void ShowIntentPickerBubble(content::WebContents* web_contents,
                            const GURL& url);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_

#include <string>

#include "chrome/browser/apps/link_capturing/intent_picker_info.h"

class GURL;

namespace content {
class WebContents;
}

namespace apps {

// Returns the size, in dp, of app icons shown in the intent picker bubble.
int GetIntentPickerBubbleIconSize();

void LaunchAppFromIntentPicker(content::WebContents* web_contents,
                               const GURL& url,
                               const std::string& launch_name,
                               apps::PickerEntryType app_type);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_

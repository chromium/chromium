// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_INTENT_PICKER_HELPERS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_INTENT_PICKER_HELPERS_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace apps {

void OnIntentPickerClosedChromeOs(
    base::WeakPtr<content::WebContents> web_contents,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist);

void LaunchAppFromIntentPickerChromeOs(content::WebContents* web_contents,
                                       const GURL& url,
                                       const std::string& launch_name,
                                       PickerEntryType app_type);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_INTENT_PICKER_HELPERS_H_

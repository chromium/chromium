// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_INTENT_PICKER_HELPERS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_INTENT_PICKER_HELPERS_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace apps {

struct NavigationInfo {
  content::WebContents* web_contents;
  GURL url;
  GURL starting_url;
  bool is_navigate_from_link;
};

void MaybeShowIntentPickerBubble(NavigationInfo navigation_info,
                                 std::vector<IntentPickerAppInfo> apps);

// These enums are used to define the intent picker show state, whether the
// picker is popped out or just displayed as a clickable omnibox icon.
enum class PickerShowState {
  kOmnibox = 1,  // Only show the intent icon in the omnibox
  kPopOut = 2,   // show the intent picker icon and pop out bubble
};

void OnIntentPickerClosedChromeOs(
    base::WeakPtr<content::WebContents> web_contents,
    PickerShowState show_state,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist);

void LaunchAppFromIntentPickerChromeOs(content::WebContents* web_contents,
                                       const GURL& url,
                                       const std::string& launch_name,
                                       PickerEntryType app_type);

bool ShouldOverrideUrlLoadingForOfficeExperiment(const GURL& previous_url,
                                                 const GURL& current_url);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_INTENT_PICKER_HELPERS_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_UTILS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_UTILS_H_

// Put utility functions only used by //chrome code here. If a function declared
// here would be meaningfully shared with other platforms, consider moving it to
// components/content_settings/core/browser/content_settings_utils.h.

namespace content {
class WebContents;
}  // namespace content

namespace content_settings {

// UMA histogram for actions that a user can perform on the pop-up blocked page
// action in the omnibox. The enum values correspond to histogram entries, so do
// not remove any existing values.
enum PopupsAction {
  POPUPS_ACTION_DISPLAYED_BLOCKED_ICON_IN_OMNIBOX = 0,
  POPUPS_ACTION_DISPLAYED_BUBBLE,
  POPUPS_ACTION_SELECTED_ALWAYS_ALLOW_POPUPS_FROM,
  POPUPS_ACTION_CLICKED_LIST_ITEM_CLICKED,
  POPUPS_ACTION_CLICKED_MANAGE_POPUPS_BLOCKING,
  POPUPS_ACTION_DISPLAYED_INFOBAR_ON_MOBILE,
  POPUPS_ACTION_CLICKED_ALWAYS_SHOW_ON_MOBILE,
  POPUPS_ACTION_COUNT
};

void RecordPopupsAction(PopupsAction action);

// Calls UpdateContentSettingsIcons on the |LocationBar| for |web_contents|.
void UpdateLocationBarUiForWebContents(content::WebContents* web_contents);

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_UTILS_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_

#include <string>

#include "base/strings/string16.h"
#include "url/gurl.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

// State of the send tab to self option in the context menu.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SendTabToSelfClickResult {
  kShowItem = 0,
  kClickItem = 1,
  kShowDeviceList = 2,
  kMaxValue = kShowDeviceList,
};

namespace send_tab_to_self {

const char kOmniboxIcon[] = "OmniboxIcon";
const char kContentMenu[] = "ContentMenu";
const char kLinkMenu[] = "LinkMenu";
const char kOmniboxMenu[] = "OmniboxMenu";
const char kTabMenu[] = "TabMenu";

enum SendTabToSelfMenuType { kTab, kOmnibox, kContent, kLink };

// Adds a new entry to SendTabToSelfModel when user clicks a target device. Will
// not show a confirmation notification if |show_notification| is false.
void CreateNewEntry(content::WebContents* tab,
                    const std::string& target_device_name,
                    const std::string& target_device_guid,
                    const GURL& link_url = GURL(),
                    bool show_notification = true);

// Adds a new entry to SendTabToSelfModel when user clicks the single valid
// device. Will be called when GetValidDeviceCount() == 1.
void ShareToSingleTarget(content::WebContents* tab,
                         const GURL& link_url = GURL());

// Records whether the user click to send a tab or link when send tab to self
// entry point is shown.
void RecordSendTabToSelfClickResult(const std::string& entry_point,
                                    SendTabToSelfClickResult state);

// Records the count of valid devices when user sees the device list.
void RecordSendTabToSelfDeviceCount(const std::string& entry_point,
                                    const int& device_count);

// Gets the count of valid device number.
size_t GetValidDeviceCount(Profile* profile);

// Gets the name of the single valid device. Will be called when
// GetValidDeviceCount() == 1.
base::string16 GetSingleTargetDeviceName(Profile* profile);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_

#include <string>

#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace send_tab_to_self {

// Adds a new entry to SendTabToSelfModel when user clicks a target device. Will
// not show a confirmation notification if |show_notification| is false.
// TODO(crbug.com/1288843): Remove the unused |target_device_name| parameter,
// and consider inlining this function since it has a single callsite now.
void CreateNewEntry(content::WebContents* tab,
                    const std::string& target_device_name,
                    const std::string& target_device_guid,
                    const GURL& link_url = GURL());

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_

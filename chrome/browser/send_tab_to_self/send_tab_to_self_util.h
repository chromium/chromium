// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

#include <optional>

#include "components/send_tab_to_self/entry_point_display_reason.h"

namespace content {
class WebContents;
}

namespace send_tab_to_self {

// |web_contents| can be null.
std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
    content::WebContents* web_contents);

// Returns true if the entry point should be shown.
bool ShouldDisplayEntryPoint(content::WebContents* web_contents);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

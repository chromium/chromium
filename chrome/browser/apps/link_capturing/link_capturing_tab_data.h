// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_TAB_DATA_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_TAB_DATA_H_

#include "build/build_config.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class WebContents;
}

namespace apps {

// Returns the WindowOpenDisposition of the link click used to open this tab.
// Returns UNKNOWN if the disposition is unavailable.
WindowOpenDisposition GetLinkCapturingSourceDisposition(
    content::WebContents* contents);

void SetLinkCapturingSourceDisposition(
    content::WebContents* contents,
    WindowOpenDisposition source_disposition);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_TAB_DATA_H_

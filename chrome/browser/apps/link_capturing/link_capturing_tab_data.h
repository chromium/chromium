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

#if BUILDFLAG(IS_CHROMEOS)
// Returns the App ID of the web app where the link that caused this tab to open
// was clicked. Returns nullptr if the link was not clicked inside a web app.
const webapps::AppId* GetLinkCapturingSourceAppId(
    content::WebContents* contents);

void SetLinkCapturingSourceAppId(content::WebContents* contents,
                                 webapps::AppId source_app_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_TAB_DATA_H_

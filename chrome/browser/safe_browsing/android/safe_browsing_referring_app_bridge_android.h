// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_REFERRING_APP_BRIDGE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_REFERRING_APP_BRIDGE_ANDROID_H_

#include "components/safe_browsing/core/browser/referring_app_info.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

// Get referring app info from Java side.
// If `get_webapk_info` is true, then the referring WebAPK's start_url and
// manifest_id will also be retrieved if the referring app is a WebAPK.
internal::ReferringAppInfo GetReferringAppInfo(
    content::WebContents* web_contents,
    bool get_webapk_info);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_REFERRING_APP_BRIDGE_ANDROID_H_

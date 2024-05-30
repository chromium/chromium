// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_REFERRING_APP_BRIDGE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_REFERRING_APP_BRIDGE_ANDROID_H_

#include "components/safe_browsing/android/referring_app_info.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

// Get referring app info from Java side.
ReferringAppInfo GetReferringAppInfo(content::WebContents* web_contents);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_REFERRING_APP_BRIDGE_ANDROID_H_

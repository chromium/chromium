// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_SCREEN_CAPTURE_PERMISSION_HANDLER_ANDROID_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_SCREEN_CAPTURE_PERMISSION_HANDLER_ANDROID_H_

#include "content/public/browser/media_stream_request.h"

namespace content {
class WebContents;
}

namespace screen_capture {
void GetScreenCapturePermissionAndroid(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback);
}

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_SCREEN_CAPTURE_PERMISSION_HANDLER_ANDROID_H_

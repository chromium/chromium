// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_MEDIA_URL_INTERCEPTOR_H_
#define ANDROID_WEBVIEW_BROWSER_AW_MEDIA_URL_INTERCEPTOR_H_

#include <stdint.h>

#include <string>

#include "base/android/jni_android.h"
#include "media/base/android/media_url_interceptor.h"

namespace android_webview {

// Interceptor to handle urls for media assets in the apk.
class AwMediaUrlInterceptor : public media::MediaUrlInterceptor {
 public:
  bool Intercept(const std::string& url,
                 int* fd,
                 int64_t* offset,
                 int64_t* size) const override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_MEDIA_URL_INTERCEPTOR_H_

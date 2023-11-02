// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/webview_jni_onload.h"

#include "android_webview/lib/aw_main_delegate.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"

namespace android_webview {

bool OnJNIOnLoadInit() {
  if (!content::android::OnJNIOnLoadInit())
    return false;

  content::SetContentMainDelegate(new android_webview::AwMainDelegate());
  return true;
}

}  // namespace android_webview

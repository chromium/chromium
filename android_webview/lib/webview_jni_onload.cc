// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/webview_jni_onload.h"

#include "android_webview/lib/aw_main_delegate.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "components/version_info/version_info_values.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"

namespace android_webview {

bool OnJNIOnLoadInit() {
  if (!content::android::OnJNIOnLoadInit())
    return false;

  base::android::SetVersionNumber(PRODUCT_VERSION);
  content::SetContentMainDelegate(new android_webview::AwMainDelegate());
  return true;
}

}  // namespace android_webview

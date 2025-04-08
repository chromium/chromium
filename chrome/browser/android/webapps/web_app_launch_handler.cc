// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebAppLaunchHandler_jni.h"

namespace webapps {
static void JNI_WebAppLaunchHandler_NotifyLaunchQueue(
    JNIEnv* env,
    content::WebContents* web_contents,
    bool start_new_navigation,
    std::string& start_url,
    std::string& package_name) {}

}  // namespace webapps

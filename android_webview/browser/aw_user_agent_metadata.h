// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_USER_AGENT_METADATA_H_
#define ANDROID_WEBVIEW_BROWSER_AW_USER_AGENT_METADATA_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace android_webview {

blink::UserAgentMetadata FromJavaAwUserAgentMetadata(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_ua_metadata);

base::android::ScopedJavaLocalRef<jobject> ToJavaAwUserAgentMetadata(
    JNIEnv* env,
    const blink::UserAgentMetadata& ua_meta_data);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_USER_AGENT_METADATA_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/finds_agent_android.h"

#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/FindsAgent_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

FindsAgentAndroid::FindsAgentAndroid() = default;

FindsAgentAndroid::~FindsAgentAndroid() = default;

void FindsAgentAndroid::OpenNotificationUrl(const GURL& url) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_FindsAgent_openNotificationUrl(env, url);
}

DEFINE_JNI(FindsAgent)

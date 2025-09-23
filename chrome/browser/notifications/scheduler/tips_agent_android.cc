// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/tips_agent_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TipsAgent_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

TipsAgentAndroid::TipsAgentAndroid() = default;

TipsAgentAndroid::~TipsAgentAndroid() = default;

void TipsAgentAndroid::ShowTipsPromo(
    notifications::TipsNotificationsFeatureType feature_type) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_TipsAgent_showTipsPromo(env, static_cast<jint>(feature_type));
}

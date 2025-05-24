// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_test_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/test/webview_instrumentation_test_native_jni/TrafficStatsTestUtil_jni.h"

namespace android_webview {

jboolean JNI_TrafficStatsTestUtil_CanGetTaggedBytes(JNIEnv* env) {
  return net::CanGetTaggedBytes();
}

jlong JNI_TrafficStatsTestUtil_GetTaggedBytes(JNIEnv* env, jint jexpected_tag) {
  return net::GetTaggedBytes(jexpected_tag);
}

}  // namespace android_webview

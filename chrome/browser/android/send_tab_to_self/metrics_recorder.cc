// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "components/send_tab_to_self/metrics_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/MetricsRecorder_jni.h"

namespace send_tab_to_self {

// Static free function declared and called directly from Java.
static void JNI_MetricsRecorder_RecordNotificationShown(JNIEnv* env) {
  RecordNotificationShown();
}

// Static free function declared and called directly from Java.
static void JNI_MetricsRecorder_RecordNotificationOpened(JNIEnv* env) {
  RecordNotificationOpened();
}

// Static free function declared and called directly from Java.
static void JNI_MetricsRecorder_RecordNotificationDismissed(JNIEnv* env) {
  RecordNotificationDismissed();
}

// Static free function declared and called directly from Java.
static void JNI_MetricsRecorder_RecordNotificationTimedOut(JNIEnv* env) {
  RecordNotificationTimedOut();
}

static void JNI_MetricsRecorder_RecordScrollPositionGenerationOutcome(
    JNIEnv* env,
    jint outcome) {
  RecordScrollPositionGenerationOutcome(
      static_cast<ScrollPositionGenerationOutcome>(outcome));
}

static void JNI_MetricsRecorder_RecordScrollPositionGenerationTime(
    JNIEnv* env,
    jlong duration_ms) {
  RecordScrollPositionGenerationTime(base::Milliseconds(duration_ms));
}

static void JNI_MetricsRecorder_RecordScrollPositionSelectorLength(
    JNIEnv* env,
    jint length) {
  RecordScrollPositionSelectorLength(static_cast<size_t>(length));
}

}  // namespace send_tab_to_self

DEFINE_JNI(MetricsRecorder)

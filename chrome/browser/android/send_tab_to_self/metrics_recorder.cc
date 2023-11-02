// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "chrome/browser/share/android/jni_headers/MetricsRecorder_jni.h"
#include "components/send_tab_to_self/metrics_util.h"

namespace send_tab_to_self {

// Static free function declared and called directly from Java.
static void JNI_MetricsRecorder_RecordSendingEvent(JNIEnv* env,
                                                   int sending_event) {
  RecordSendingEvent(ShareEntryPoint::kShareSheet,
                     static_cast<SendingEvent>(sending_event));
}

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

}  // namespace send_tab_to_self

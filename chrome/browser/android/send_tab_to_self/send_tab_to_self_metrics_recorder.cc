// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_scroll_observer.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SendTabToSelfMetricsRecorder_jni.h"

namespace send_tab_to_self {

static void JNI_SendTabToSelfMetricsRecorder_RecordNotificationShown(
    JNIEnv* env) {
  RecordNotificationShown();
}

static void JNI_SendTabToSelfMetricsRecorder_AttachScrollObserver(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_web_contents,
    jboolean has_scroll_position) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (web_contents) {
    SendTabToSelfScrollObserver::CreateForWebContents(web_contents,
                                                      has_scroll_position);
  }
}

static void JNI_SendTabToSelfMetricsRecorder_RecordHasScrollPositionOnOpened(
    JNIEnv* env,
    jboolean has_scroll_position) {
  RecordHasScrollPositionOnOpened(has_scroll_position);
}

static void JNI_SendTabToSelfMetricsRecorder_RecordNotificationOpened(
    JNIEnv* env) {
  RecordNotificationOpened();
}

static void JNI_SendTabToSelfMetricsRecorder_RecordNotificationDismissed(
    JNIEnv* env) {
  RecordNotificationDismissed();
}

static void JNI_SendTabToSelfMetricsRecorder_RecordNotificationTimedOut(
    JNIEnv* env) {
  RecordNotificationTimedOut();
}

static void
JNI_SendTabToSelfMetricsRecorder_RecordScrollPositionGenerationOutcome(
    JNIEnv* env,
    jint outcome) {
  RecordScrollPositionGenerationOutcome(
      static_cast<ScrollPositionGenerationOutcome>(outcome));
}

static void JNI_SendTabToSelfMetricsRecorder_RecordScrollPositionGenerationTime(
    JNIEnv* env,
    jlong duration_ms) {
  RecordScrollPositionGenerationTime(base::Milliseconds(duration_ms));
}

static void JNI_SendTabToSelfMetricsRecorder_RecordScrollPositionSelectorLength(
    JNIEnv* env,
    jint length) {
  RecordScrollPositionSelectorLength(static_cast<size_t>(length));
}

}  // namespace send_tab_to_self

DEFINE_JNI(SendTabToSelfMetricsRecorder)

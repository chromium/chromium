// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_scroll_observer.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SendTabToSelfMetricsRecorder_jni.h"

namespace send_tab_to_self {

static void JNI_SendTabToSelfMetricsRecorder_RecordNotificationStatus(
    JNIEnv* env,
    NotificationStatus status) {
  RecordNotificationStatus(status);
  if (status == NotificationStatus::kOpened) {
    RecordAutoOpenOutcome(AutoOpenOutcome::kTabOpenedViaNotification);
  }
}

static void JNI_SendTabToSelfMetricsRecorder_AttachScrollObserver(
    JNIEnv* env,
    content::WebContents* web_contents,
    jboolean has_scroll_position) {
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

static void
JNI_SendTabToSelfMetricsRecorder_RecordScrollPositionGenerationOutcome(
    JNIEnv* env,
    ScrollPositionGenerationOutcome outcome) {
  RecordScrollPositionGenerationOutcome(outcome);
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

static void JNI_SendTabToSelfMetricsRecorder_RecordEntryPointInvoked(
    JNIEnv* env,
    ShareEntryPoint entry_point) {
  RecordEntryPointInvoked(entry_point);
}

}  // namespace send_tab_to_self

DEFINE_JNI(SendTabToSelfMetricsRecorder)

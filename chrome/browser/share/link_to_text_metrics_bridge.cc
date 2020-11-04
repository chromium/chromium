// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/share/android/jni_headers/LinkToTextMetricsBridge_jni.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"

static void JNI_LinkToTextMetricsBridge_LogGenerateErrorTabHidden(JNIEnv* env) {
  shared_highlighting::LogGenerateErrorTabHidden();
}

static void JNI_LinkToTextMetricsBridge_LogGenerateErrorOmniboxNavigation(
    JNIEnv* env) {
  shared_highlighting::LogGenerateErrorOmniboxNavigation();
}

static void JNI_LinkToTextMetricsBridge_LogGenerateErrorTabCrash(JNIEnv* env) {
  shared_highlighting::LogGenerateErrorTabCrash();
}

static void JNI_LinkToTextMetricsBridge_LogGenerateErrorIFrame(JNIEnv* env) {
  shared_highlighting::LogGenerateErrorIFrame();
}

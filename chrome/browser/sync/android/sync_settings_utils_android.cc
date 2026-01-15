// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SyncSettingsUtils_jni.h"
#include "chrome/browser/sync/sync_ui_util.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

ScopedJavaLocalRef<jstring>
JNI_SyncSettingsUtils_GetBookmarksLimitExceededHelpUrl(JNIEnv* env) {
  return ConvertUTF8ToJavaString(env, kBookmarksLimitExceededHelpCenter);
}

DEFINE_JNI(SyncSettingsUtils)

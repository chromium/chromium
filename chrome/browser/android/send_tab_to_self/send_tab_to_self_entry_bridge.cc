// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/send_tab_to_self_entry_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SendTabToSelfEntry_jni.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace send_tab_to_self {

ScopedJavaLocalRef<jobject> CreateJavaSendTabToSelfEntry(
    JNIEnv* env,
    const SendTabToSelfEntry* entry) {
  return Java_SendTabToSelfEntry_createSendTabToSelfEntry(
      env, ConvertUTF8ToJavaString(env, entry->GetGUID()),
      ConvertUTF8ToJavaString(env, entry->GetURL().spec()),
      ConvertUTF8ToJavaString(env, entry->GetTitle()),
      entry->GetSharedTime().ToJavaTime(),
      entry->GetOriginalNavigationTime().ToJavaTime(),
      ConvertUTF8ToJavaString(env, entry->GetDeviceName()),
      ConvertUTF8ToJavaString(env, entry->GetTargetDeviceSyncCacheGuid()));
}

}  // namespace send_tab_to_self

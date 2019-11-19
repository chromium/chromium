// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/macros.h"

namespace send_tab_to_self {
class SendTabToSelfEntry;

// Function to convert the native version of SendTabToSelfEntry into the Java
// version.
base::android::ScopedJavaLocalRef<jobject> CreateJavaSendTabToSelfEntry(
    JNIEnv* env,
    const SendTabToSelfEntry* entry);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_BRIDGE_H_

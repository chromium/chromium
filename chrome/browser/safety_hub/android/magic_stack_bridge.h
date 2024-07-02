// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFETY_HUB_ANDROID_MAGIC_STACK_BRIDGE_H_
#define CHROME_BROWSER_SAFETY_HUB_ANDROID_MAGIC_STACK_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service.h"

base::android::ScopedJavaLocalRef<jobject> ToJavaMagicStackEntry(
    JNIEnv* env,
    const MenuNotificationEntry& obj);

namespace jni_zero {

template <>
inline ScopedJavaLocalRef<jobject> ToJniType(JNIEnv* env,
                                             const MenuNotificationEntry& obj) {
  return ToJavaMagicStackEntry(env, obj);
}

}  // namespace jni_zero

#endif  // CHROME_BROWSER_SAFETY_HUB_ANDROID_MAGIC_STACK_BRIDGE_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/timezone_utils.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/TimezoneUtils_jni.h"

namespace base {
namespace android {

std::u16string GetDefaultTimeZoneId() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> timezone_id =
      Java_TimezoneUtils_getDefaultTimeZoneId(env);
  return ConvertJavaStringToUTF16(timezone_id);
}

}  // namespace android
}  // namespace base

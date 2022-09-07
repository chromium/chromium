// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/android/url_utils.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/test/test_support_jni_headers/UrlUtils_jni.h"

namespace base {
namespace android {

FilePath GetIsolatedTestRoot() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jtest_data_dir =
      Java_UrlUtils_getIsolatedTestRoot(env);
  base::FilePath test_data_dir(
      base::android::ConvertJavaStringToUTF8(env, jtest_data_dir));
  return test_data_dir;
}

}  // namespace android
}  // namespace base

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/android/chrome_test_util_jni/PwaRestoreBottomSheetTestUtils_jni.h"

#include <string>
#include <vector>

using base::android::JavaParamRef;

namespace webapps {

void JNI_PwaRestoreBottomSheetTestUtils_SetAppListForRestoring(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& apps) {
  std::vector<std::vector<std::u16string>> app_vector;
  Java2dStringArrayTo2dStringVector(env, apps, &app_vector);

  // TODO(finnur): Manipulate the WebApkDatabase with these values, once it has
  // been added as a KeyedService.
  for (std::vector<std::u16string> app : app_vector) {
    LOG(WARNING) << "App id: " << app[0] << " Name: " << app[1];
  }
}

}  // namespace webapps

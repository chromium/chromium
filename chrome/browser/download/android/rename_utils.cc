// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "chrome/android/chrome_jni_headers/RenameUtils_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// static
static base::android::ScopedJavaLocalRef<jstring>
JNI_RenameUtils_GetFileExtension(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& fileName) {
  std::string extension =
      base::FilePath(ConvertJavaStringToUTF8(env, fileName)).Extension();
  return ConvertUTF8ToJavaString(env, extension);
}

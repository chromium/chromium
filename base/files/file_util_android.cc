// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base_jni_headers/FileUtils_jni.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace base {
namespace android {

static ScopedJavaLocalRef<jstring> JNI_FileUtils_GetAbsoluteFilePath(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_file_path) {
  base::FilePath file_path(
      base::android::ConvertJavaStringToUTF8(env, j_file_path));
  base::FilePath absolute_file_path = MakeAbsoluteFilePath(file_path);
  return base::android::ConvertUTF8ToJavaString(env,
                                                absolute_file_path.value());
}

}  // namespace android

bool GetShmemTempDir(bool executable, base::FilePath* path) {
  return PathService::Get(base::DIR_CACHE, path);
}

}  // namespace base

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base_jni/FileUtils_jni.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace base {
namespace android {

static base::FilePath JNI_FileUtils_GetAbsoluteFilePath(
    JNIEnv* env,
    base::FilePath& file_path) {
  return MakeAbsoluteFilePath(file_path);
}

}  // namespace android

bool GetShmemTempDir(bool executable, base::FilePath* path) {
  return PathService::Get(base::DIR_CACHE, path);
}

}  // namespace base

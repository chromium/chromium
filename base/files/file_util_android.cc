// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/FileUtils_jni.h"

namespace base {
namespace android {

static std::string JNI_FileUtils_GetAbsoluteFilePath(JNIEnv* env,
                                                     std::string& file_path) {
  return MakeAbsoluteFilePath(base::FilePath(file_path)).value();
}

}  // namespace android

bool GetShmemTempDir(bool executable, base::FilePath* path) {
  return PathService::Get(base::DIR_CACHE, path);
}

}  // namespace base

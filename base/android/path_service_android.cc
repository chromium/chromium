// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base_jni/PathService_jni.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

namespace base {
namespace android {

void JNI_PathService_Override(JNIEnv* env, jint what, std::string& path) {
  PathService::Override(what, FilePath(path));
}

}  // namespace android
}  // namespace base

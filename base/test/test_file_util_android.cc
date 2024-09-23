// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/test/base_unittests_jni_headers/ContentUriTestUtils_jni.h"

using base::android::ScopedJavaLocalRef;

namespace base {

FilePath InsertImageIntoMediaStore(const FilePath& path) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_path =
      base::android::ConvertUTF8ToJavaString(env, path.value());
  ScopedJavaLocalRef<jstring> j_uri =
      Java_ContentUriTestUtils_insertImageIntoMediaStore(env, j_path);
  std::string uri = base::android::ConvertJavaStringToUTF8(j_uri);
  return FilePath(uri);
}

}  // namespace base

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_asset_reader.h"

#include "android_webview/browser_jni_headers/AwAssetReader_jni.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"

namespace android_webview {

int AwAssetReader::OpenApkAsset(const std::string& file_path,
                                base::MemoryMappedFile::Region* region) {
  // The AssetManager API of the NDK does not expose a method for accessing raw
  // resources :(
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jlongArray> jarr = Java_AwAssetReader_open(
      env, base::android::ConvertUTF8ToJavaString(env, file_path));
  std::vector<jlong> results;
  base::android::JavaLongArrayToLongVector(env, jarr, &results);
  CHECK_EQ(3U, results.size());
  int fd = static_cast<int>(results[0]);
  region->offset = results[1];
  // Not a checked_cast because open() may return -1.
  region->size = static_cast<size_t>(results[2]);
  return fd;
}

}  // namespace android_webview

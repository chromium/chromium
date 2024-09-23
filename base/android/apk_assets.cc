// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/apk_assets.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/file_descriptor_store.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/ApkAssets_jni.h"

namespace base {
namespace android {

int OpenApkAsset(const std::string& file_path,
                 const std::string& split_name,
                 base::MemoryMappedFile::Region* region) {
  // The AssetManager API of the NDK does not expose a method for accessing raw
  // resources :(
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jlongArray> jarr =
      Java_ApkAssets_open(env, ConvertUTF8ToJavaString(env, file_path),
                          ConvertUTF8ToJavaString(env, split_name));
  std::vector<jlong> results;
  base::android::JavaLongArrayToLongVector(env, jarr, &results);
  CHECK_EQ(3U, results.size());
  int fd = static_cast<int>(results[0]);
  region->offset = results[1];
  // Not a checked_cast because open() may return -1.
  region->size = static_cast<size_t>(results[2]);
  return fd;
}

int OpenApkAsset(const std::string& file_path,
                 base::MemoryMappedFile::Region* region) {
  return OpenApkAsset(file_path, std::string(), region);
}

bool RegisterApkAssetWithFileDescriptorStore(const std::string& key,
                                             const base::FilePath& file_path) {
  base::MemoryMappedFile::Region region =
      base::MemoryMappedFile::Region::kWholeFile;
  int asset_fd = OpenApkAsset(file_path.value(), &region);
  if (asset_fd == -1)
    return false;
  base::FileDescriptorStore::GetInstance().Set(key, base::ScopedFD(asset_fd),
                                               region);
  return true;
}

void DumpLastOpenApkAssetFailure() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> error =
      Java_ApkAssets_takeLastErrorString(env);
  if (!error) {
    return;
  }
  SCOPED_CRASH_KEY_STRING256("base", "OpenApkAssetError",
                             ConvertJavaStringToUTF8(env, error));
  base::debug::DumpWithoutCrashing();
}

}  // namespace android
}  // namespace base

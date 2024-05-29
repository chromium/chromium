// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <memory>
#include <string>

#include "android_webview/browser/variations/variations_seed_loader.h"

#include "android_webview/proto/aw_variations_seed.pb.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/VariationsSeedLoader_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace android_webview {

AwVariationsSeed* g_seed = nullptr;

namespace {
bool IsSeedValid(AwVariationsSeed* seed) {
  // Empty or incomplete protos should be considered invalid. An empty seed
  // file is expected when we request a seed from the service, but no new seed
  // is available. In that case, an empty seed file will have been created, but
  // never written to.
  if (!seed->has_signature()) {
    LOG(ERROR) << "Seed missing signature.";
    return false;
  } else if (!seed->has_date()) {
    LOG(ERROR) << "Seed missing date.";
    return false;
  } else if (!seed->has_country()) {
    LOG(ERROR) << "Seed missing country.";
    return false;
  } else if (!seed->has_is_gzip_compressed()) {
    LOG(ERROR) << "Seed not compressed.";
    return false;
  } else if (!seed->has_seed_data()) {
    LOG(ERROR) << "Seed missing data.";
    return false;
  }
  return true;
}
}  // namespace

static jboolean JNI_VariationsSeedLoader_ParseAndSaveSeedProto(
    JNIEnv* env,
    const JavaParamRef<jstring>& seed_path) {
  // Parse the proto.
  std::unique_ptr<AwVariationsSeed> seed =
      std::make_unique<AwVariationsSeed>(AwVariationsSeed::default_instance());
  std::string native_seed_path = ConvertJavaStringToUTF8(seed_path);

  int native_fd = open(native_seed_path.c_str(), O_RDONLY);
  if (native_fd == -1) {
    PLOG(INFO) << "Failed to open file for reading.";
    return false;
  }

  base::ScopedFD seed_fd(native_fd);
  if (!seed_fd.get()) {
    LOG(ERROR) << "Failed to create seed file descriptor.";
    return false;
  }

  if (!seed->ParseFromFileDescriptor(seed_fd.get())) {
    LOG(ERROR) << "Falied to parse seed file.";
    return false;
  }

  if (IsSeedValid(seed.get())) {
    g_seed = seed.release();
    return true;
  } else {
    return false;
  }
}

static jboolean JNI_VariationsSeedLoader_ParseAndSaveSeedProtoFromByteArray(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& seed_as_bytes) {
  // Parse the proto.
  std::unique_ptr<AwVariationsSeed> seed =
      std::make_unique<AwVariationsSeed>(AwVariationsSeed::default_instance());
  jbyte* src_bytes = env->GetByteArrayElements(seed_as_bytes, nullptr);
  if (!seed->ParseFromArray(src_bytes,
                            env->GetArrayLength(seed_as_bytes.obj()))) {
    LOG(ERROR) << "Failed to parse seed file.";
    return false;
  }

  if (IsSeedValid(seed.get())) {
    g_seed = seed.release();
    return true;
  } else {
    return false;
  }
}

static jlong JNI_VariationsSeedLoader_GetSavedSeedDate(JNIEnv* env) {
  return g_seed ? g_seed->date() : 0;
}

std::unique_ptr<AwVariationsSeed> TakeSeed() {
  std::unique_ptr<AwVariationsSeed> seed(g_seed);
  g_seed = nullptr;
  return seed;
}

void CacheSeedFreshness(long freshness) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_VariationsSeedLoader_cacheSeedFreshness(env, freshness);
}

}  // namespace android_webview

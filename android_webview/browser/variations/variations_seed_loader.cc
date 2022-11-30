// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <jni.h>
#include <string>

#include "android_webview/browser/variations/variations_seed_loader.h"

#include "android_webview/browser_jni_headers/VariationsSeedLoader_jni.h"
#include "android_webview/proto/aw_variations_seed.pb.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace android_webview {

AwVariationsSeed* g_seed = nullptr;

static jboolean JNI_VariationsSeedLoader_ParseAndSaveSeedProto(
    JNIEnv* env,
    const JavaParamRef<jstring>& seed_path) {
  // Parse the proto.
  std::unique_ptr<AwVariationsSeed> seed =
      std::make_unique<AwVariationsSeed>(AwVariationsSeed::default_instance());
  std::string native_seed_path = ConvertJavaStringToUTF8(seed_path);
  base::ScopedFD seed_fd(open(native_seed_path.c_str(), O_RDONLY));
  if (!seed->ParseFromFileDescriptor(seed_fd.get())) {
    return false;
  }

  // Empty or incomplete protos should be considered invalid. An empty seed
  // file is expected when we request a seed from the service, but no new seed
  // is available. In that case, an empty seed file will have been created, but
  // never written to.
  if (!seed->has_signature() || !seed->has_date() || !seed->has_country() ||
      !seed->has_is_gzip_compressed() || !seed->has_seed_data()) {
    return false;
  }
  g_seed = seed.release();
  return true;
}

static jlong JNI_VariationsSeedLoader_GetSavedSeedDate(JNIEnv* env) {
  return g_seed ? g_seed->date() : 0;
}

std::unique_ptr<AwVariationsSeed> TakeSeed() {
  std::unique_ptr<AwVariationsSeed> seed(g_seed);
  g_seed = nullptr;
  return seed;
}

}  // namespace android_webview

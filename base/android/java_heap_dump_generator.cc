// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/java_heap_dump_generator.h"

#include <jni.h>

#include <string_view>

#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/memory_jni/JavaHeapDumpGenerator_jni.h"

namespace base {
namespace android {

bool WriteJavaHeapDumpToPath(std::string_view filePath) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_JavaHeapDumpGenerator_generateHprof(
      env, base::android::ConvertUTF8ToJavaString(env, filePath));
}

}  // namespace android
}  // namespace base

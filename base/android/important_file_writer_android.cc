// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/important_file_writer.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/threading/thread_restrictions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/ImportantFileWriterAndroid_jni.h"

namespace base {
namespace android {

class ScopedAllowBlockingForImportantFileWriter
    : public base::ScopedAllowBlocking {};

static jboolean JNI_ImportantFileWriterAndroid_WriteFileAtomically(
    JNIEnv* env,
    std::string& native_file_name,
    jni_zero::ByteArrayView& data) {
  // This is called on the UI thread during shutdown to save tab data, so
  // needs to enable IO.
  ScopedAllowBlockingForImportantFileWriter allow_blocking;
  base::FilePath path(native_file_name);
  bool result =
      base::ImportantFileWriter::WriteFileAtomically(path, data.string_view());
  return result;
}

}  // namespace android
}  // namespace base

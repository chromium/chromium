// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_android.h"
#include "base/android/library_loader/anchor_functions_buildflags.h"
#include "base/android/library_loader/library_prefetcher.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/library_loader_jni/LibraryPrefetcher_jni.h"

namespace base {
namespace android {

static void JNI_LibraryPrefetcher_PrefetchNativeLibraryForWebView(JNIEnv* env) {
#if BUILDFLAG(SUPPORTS_CODE_ORDERING)
  return NativeLibraryPrefetcher::PrefetchNativeLibrary();
#endif
}


}  // namespace android
}  // namespace base

DEFINE_JNI(LibraryPrefetcher)

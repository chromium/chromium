// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "chrome/browser/android/tab_web_contents_destroyer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_test_util_jni/TabWebContentsDestroyerCleanupHook_jni.h"

namespace tabs {

static int32_t JNI_TabWebContentsDestroyerCleanupHook_GetActiveInstanceCount(
    JNIEnv* env) {
  return TabWebContentsDestroyer::
      GetActiveInstanceCountForTesting();  // IN-TEST
}

}  // namespace tabs

DEFINE_JNI(TabWebContentsDestroyerCleanupHook)

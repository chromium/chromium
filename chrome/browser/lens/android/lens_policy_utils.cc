// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/lens/android/lens_prefs.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/LensPolicyUtils_jni.h"

static jboolean JNI_LensPolicyUtils_GetLensCameraAssistedSearchEnabled(
    JNIEnv* env) {
  return g_browser_process->local_state()->GetBoolean(
      lens::kLensCameraAssistedSearchEnabled);
}

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/register_jni.h"

#include "base/android/jni_utils.h"
#include "chrome/browser/android/vr/register_gvr_jni.h"

#if defined(JNI_REGISTRATION_FOR_TESTING)
#include "chrome/browser/android/vr/jni_registration_for_testing_generated.h"  // nogncheck
#else
#include "chrome/browser/android/vr/jni_registration_generated.h"  // nogncheck
#endif  // defined(JNI_REGISTRATION_FOR_TESTING)

namespace vr {

bool RegisterJni(JNIEnv* env) {
  if (!base::android::IsSelectiveJniRegistrationEnabled(env) &&
      !vr::RegisterNonMainDexNatives(env)) {
    return false;
  }
  if (!vr::RegisterMainDexNatives(env)) {
    return false;
  }
  if (!vr::RegisterGvrJni(env)) {
    return false;
  }
  return true;
}

}  // namespace vr

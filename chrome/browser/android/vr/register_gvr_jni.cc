// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/register_gvr_jni.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_utils.h"
#include "third_party/gvr-android-sdk/display_synchronizer_jni.h"
#include "third_party/gvr-android-sdk/gvr_api_jni.h"
#include "third_party/gvr-android-sdk/native_callbacks_jni.h"

namespace vr {

// These VR native functions are not handled by the automatic registration, so
// they are manually registered here.
static const base::android::RegistrationMethod kGvrRegisteredMethods[] = {
    {"DisplaySynchronizer",
     DisplaySynchronizer::RegisterDisplaySynchronizerNatives},
    {"GvrApi", GvrApi::RegisterGvrApiNatives},
    {"NativeCallbacks", NativeCallbacks::RegisterNativeCallbacksNatives},
};

bool RegisterGvrJni(JNIEnv* env) {
  if (!RegisterNativeMethods(env, kGvrRegisteredMethods,
                             std::size(kGvrRegisteredMethods))) {
    return false;
  }
  return true;
}

}  // namespace vr

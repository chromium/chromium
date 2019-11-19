// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_core_info.h"

#include "base/android/jni_android.h"
#include "chrome/android/features/vr/jni_headers/VrCoreInfo_jni.h"

using base::android::JavaParamRef;

namespace vr {

VrCoreInfo::VrCoreInfo(int32_t major_version,
                       int32_t minor_version,
                       int32_t patch_version,
                       VrCoreCompatibility compatibility)
    : gvr_sdk_version({major_version, minor_version, patch_version}),
      compatibility(compatibility) {}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_VrCoreInfo_Init(JNIEnv* env,
                          const JavaParamRef<jobject>& obj,
                          jint major_version,
                          jint minor_version,
                          jint patch_version,
                          jint compatibility) {
  return reinterpret_cast<intptr_t>(
      new VrCoreInfo(major_version, minor_version, patch_version,
                     static_cast<VrCoreCompatibility>(compatibility)));
}

}  // namespace vr

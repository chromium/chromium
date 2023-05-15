// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/register_jni.h"

#include "chrome/browser/android/vr/register_gvr_jni.h"
#include "device/vr/buildflags/buildflags.h"

namespace vr {

bool RegisterJni(JNIEnv* env) {
  // The GVR Java code will be in the vr DFM, which is loaded as an isolated
  // split.  This means the Java code is not automatically loaded in the base
  // class loader. Automatic JNI registration only works for native methods
  // associated with the base class loader (which loaded libmonochrome.so, so
  // will look for symbols there). Most of Chrome's native methods are in
  // GEN_JNI.java which is present in the base module, so do not need manual
  // registration. Since GVR has native methods outside of GEN_JNI.java which
  // are not present in the base module, these must be manually registered.
#if BUILDFLAG(ENABLE_GVR_SERVICES)
  if (!vr::RegisterGvrJni(env)) {
    return false;
  }
#endif

  return true;
}

}  // namespace vr

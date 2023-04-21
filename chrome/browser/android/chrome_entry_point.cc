// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "chrome/app/android/chrome_jni_onload.h"

namespace {

bool NativeInit(base::android::LibraryProcessType) {
  return android::OnJNIOnLoadInit();
}

}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  base::android::SetNativeInitializationHook(NativeInit);
  return JNI_VERSION_1_4;
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"

// This does not live in the "base" component, but rather belongs to a
// source_set that must be included in the root component of a shared library.
// Component build requires the OnLoad symbol to be available in the root
// component.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  base::android::SetNativeInitializationHook(NativeInitializationHook);
  return JNI_VERSION_1_4;
}

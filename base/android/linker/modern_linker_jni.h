// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_LINKER_MODERN_LINKER_JNI_H_
#define BASE_ANDROID_LINKER_MODERN_LINKER_JNI_H_

#include <jni.h>

namespace chromium_android_linker {

// Used to find out whether RELRO sharing is often rejected due to mismatch of
// the contents.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Must be kept in sync with the enum
// in enums.xml. A java @IntDef is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.base.library_loader
enum class RelroSharingStatus {
  NOT_ATTEMPTED = 0,
  SHARED = 1,
  NOT_IDENTICAL = 2,
  COUNT = 3,
};

// JNI_OnLoad() initialization hook for the modern linker.
// Sets up JNI and other initializations for native linker code.
// |vm| is the Java VM handle passed to JNI_OnLoad().
// |env| is the current JNI environment handle.
// On success, returns true.
extern bool ModernLinkerJNIInit(JavaVM* vm, JNIEnv* env);

}  // namespace chromium_android_linker

#endif  // BASE_ANDROID_LINKER_MODERN_LINKER_JNI_H_

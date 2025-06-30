// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_ONLOAD_H_
#define BASE_ANDROID_JNI_ONLOAD_H_

#include "base/android/library_loader/library_loader_hooks.h"

// The JNI_OnLoad in //base cannot depend on any specific process type's init
// function, so we have this hook that we compile different implementations
// for depending on what shared library we are building.
bool NativeInitializationHook(base::android::LibraryProcessType value);

#endif  // BASE_ANDROID_JNI_ONLOAD_H_

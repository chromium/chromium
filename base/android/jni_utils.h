// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_UTILS_H_
#define BASE_ANDROID_JNI_UTILS_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace base {

namespace android {

// Gets a ClassLoader instance capable of loading Chromium java classes.
// This should be called either from JNI_OnLoad or from within a method called
// via JNI from Java.
BASE_EXPORT ScopedJavaLocalRef<jobject> GetClassLoader(JNIEnv* env);

// Gets a ClassLoader instance which can load Java classes from the specified
// split.
BASE_EXPORT ScopedJavaLocalRef<jobject> GetSplitClassLoader(
    JNIEnv* env,
    const std::string& split_name);

// Returns true if the current process permits selective JNI registration.
BASE_EXPORT bool IsSelectiveJniRegistrationEnabled(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JNI_UTILS_H_


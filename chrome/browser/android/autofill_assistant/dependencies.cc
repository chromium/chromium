// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/dependencies.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "chrome/android/features/autofill_assistant/jni_headers_public/AssistantStaticDependencies_jni.h"

using ::base::android::AttachCurrentThread;
using ::base::android::JavaParamRef;
using ::base::android::ScopedJavaGlobalRef;

namespace autofill_assistant {

std::unique_ptr<Dependencies> Dependencies::CreateFromJavaObject(
    ScopedJavaGlobalRef<jobject> java_object) {
  const jlong object_ptr = Java_AssistantStaticDependencies_getNativePointer(
      AttachCurrentThread(), java_object);
  return base::WrapUnique(reinterpret_cast<Dependencies*>(object_ptr));
}

Dependencies::Dependencies(JNIEnv* env,
                           const JavaParamRef<jobject>& java_object)
    : java_object_(java_object) {}

ScopedJavaGlobalRef<jobject> Dependencies::GetJavaObject() const {
  return java_object_;
}

ScopedJavaGlobalRef<jobject> Dependencies::GetInfoPageUtil(
    const ScopedJavaGlobalRef<jobject>& java_object) {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_getInfoPageUtil(AttachCurrentThread(),
                                                       java_object));
}

ScopedJavaGlobalRef<jobject> Dependencies::GetAccessTokenUtil() const {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_getAccessTokenUtil(AttachCurrentThread(),
                                                          java_object_));
}

Dependencies::~Dependencies() = default;

}  // namespace autofill_assistant

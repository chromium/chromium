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

std::unique_ptr<Dependencies> Dependencies::CreateFromJavaDependencies(
    ScopedJavaGlobalRef<jobject> jdependencies) {
  std::unique_ptr<Dependencies> dependencies =
      CreateFromJavaStaticDependencies(jdependencies);
  dependencies->SetJavaDependencies(jdependencies);
  return dependencies;
}

std::unique_ptr<Dependencies> Dependencies::CreateFromJavaStaticDependencies(
    ScopedJavaGlobalRef<jobject> jstatic_dependencies) {
  return base::WrapUnique(reinterpret_cast<Dependencies*>(
      Java_AssistantStaticDependencies_createNative(AttachCurrentThread(),
                                                    jstatic_dependencies)));
}

Dependencies::Dependencies(JNIEnv* env,
                           const JavaParamRef<jobject>& jstatic_dependencies)
    : jstatic_dependencies_(jstatic_dependencies) {}

void Dependencies::SetJavaDependencies(
    base::android::ScopedJavaGlobalRef<jobject> jdependencies) {
  jdependencies_ = jdependencies;
}

ScopedJavaGlobalRef<jobject> Dependencies::GetJavaStaticDependencies() const {
  return jstatic_dependencies_;
}

ScopedJavaGlobalRef<jobject> Dependencies::GetJavaDependencies() const {
  return jdependencies_;
}

ScopedJavaGlobalRef<jobject> Dependencies::CreateInfoPageUtil(
    const ScopedJavaGlobalRef<jobject>& jstatic_dependencies) {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_createInfoPageUtil(
          AttachCurrentThread(), jstatic_dependencies));
}

ScopedJavaGlobalRef<jobject> Dependencies::CreateAccessTokenUtil() const {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_createAccessTokenUtil(
          AttachCurrentThread(), jstatic_dependencies_));
}

Dependencies::~Dependencies() = default;

}  // namespace autofill_assistant

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/android/autofill_assistant/dependencies_android_chrome.h"

#include "base/android/scoped_java_ref.h"
#include "chrome/android/features/autofill_assistant/jni_headers_public/AssistantStaticDependenciesChrome_jni.h"

namespace autofill_assistant {

using ::base::android::JavaParamRef;

static jlong JNI_AssistantStaticDependenciesChrome_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jstatic_dependencies) {
  // The dynamic_cast is necessary here to safely cast the resulting intptr back
  // to DependenciesAndroid using reinterpret_cast.
  // TODO(b/222671580): remove dynamic_cast.
  return reinterpret_cast<intptr_t>(dynamic_cast<DependenciesAndroid*>(
      new DependenciesAndroidChrome(env, jstatic_dependencies)));
}

DependenciesAndroidChrome::DependenciesAndroidChrome(
    JNIEnv* env,
    const JavaParamRef<jobject>& jstatic_dependencies)
    : DependenciesAndroid(env, jstatic_dependencies) {}

const CommonDependencies* DependenciesAndroidChrome::GetCommonDependencies()
    const {
  return &common_dependencies_;
}
const PlatformDependencies* DependenciesAndroidChrome::GetPlatformDependencies()
    const {
  return &platform_dependencies_;
}

}  // namespace autofill_assistant

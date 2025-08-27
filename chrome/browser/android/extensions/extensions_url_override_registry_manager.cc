// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/extensions/extensions_url_override_registry_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/numerics/safe_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from ExtensionsUrlOverrideRegistryManager.java.
#include "chrome/browser/android/extensions/jni_headers/ExtensionsUrlOverrideRegistryManager_jni.h"

namespace extensions {
static jlong JNI_ExtensionsUrlOverrideRegistryManager_Initialize(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_java_object) {
  ExtensionsUrlOverrideRegistryManager* extensions_url_override_manager =
      new ExtensionsUrlOverrideRegistryManager();
  return reinterpret_cast<intptr_t>(extensions_url_override_manager);
}

void ExtensionsUrlOverrideRegistryManager::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace extensions

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/extensions/extensions_url_override_registry_manager.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/android/extensions/extensions_url_override_state_tracker_impl.h"
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from ExtensionsUrlOverrideRegistryManager.java.
#include "chrome/browser/android/extensions/jni_headers/ExtensionsUrlOverrideRegistryManager_jni.h"

namespace extensions {

static jlong JNI_ExtensionsUrlOverrideRegistryManager_Initialize(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_object,
    Profile* profile) {
  ExtensionsUrlOverrideRegistryManager* extensions_url_override_manager =
      new ExtensionsUrlOverrideRegistryManager(env, j_object, profile);
  return reinterpret_cast<intptr_t>(extensions_url_override_manager);
}

void ExtensionsUrlOverrideRegistryManager::OnUrlOverrideEnabled(
    const std::string& chrome_url_path,
    bool incognito_enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsUrlOverrideRegistryManager_onUrlOverrideEnabled(
      env, j_object_, chrome_url_path, incognito_enabled);
}

void ExtensionsUrlOverrideRegistryManager::OnUrlOverrideDisabled(
    const std::string& chrome_url_path) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionsUrlOverrideRegistryManager_onUrlOverrideDisabled(
      env, j_object_, chrome_url_path);
}

ExtensionsUrlOverrideRegistryManager::ExtensionsUrlOverrideRegistryManager(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_object,
    Profile* profile)
    : j_object_(env, j_object) {
  if (!base::FeatureList::IsEnabled(
          chrome::android::kChromeNativeUrlOverriding)) {
    return;
  }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  state_tracker_ =
      std::make_unique<ExtensionUrlOverrideStateTrackerImpl>(profile, this);
#endif
}

ExtensionsUrlOverrideRegistryManager::~ExtensionsUrlOverrideRegistryManager() =
    default;

void ExtensionsUrlOverrideRegistryManager::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace extensions

DEFINE_JNI(ExtensionsUrlOverrideRegistryManager)

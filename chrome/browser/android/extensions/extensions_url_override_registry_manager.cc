// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/extensions/extensions_url_override_registry_manager.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/numerics/safe_conversions.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/android/extensions/extensions_url_override_state_tracker_impl.h"
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from ExtensionsUrlOverrideRegistryManager.java.
#include "chrome/browser/android/extensions/jni_headers/ExtensionsUrlOverrideRegistryManager_jni.h"

namespace extensions {

static jlong JNI_ExtensionsUrlOverrideRegistryManager_Initialize(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_java_object,
    Profile* profile) {
  ExtensionsUrlOverrideRegistryManager* extensions_url_override_manager =
      new ExtensionsUrlOverrideRegistryManager(profile);
  return reinterpret_cast<intptr_t>(extensions_url_override_manager);
}

void ExtensionsUrlOverrideRegistryManager::OnUrlOverrideEnabled(
    const std::string& chrome_url_path,
    bool incognito_enabled) {}

void ExtensionsUrlOverrideRegistryManager::OnUrlOverrideDisabled(
    const std::string& chrome_url_path) {}

ExtensionsUrlOverrideRegistryManager::ExtensionsUrlOverrideRegistryManager(
    Profile* profile) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
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

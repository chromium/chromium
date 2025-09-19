// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_REGISTRY_MANAGER_H_
#define CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_REGISTRY_MANAGER_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "chrome/browser/android/extensions/extensions_url_override_state_tracker.h"

namespace extensions {
// Listens to changes to the Native-level extensions URL registry and handles
// updates to Android classes.
class ExtensionsUrlOverrideRegistryManager
    : public ExtensionUrlOverrideStateTracker::StateListener {
 public:
  explicit ExtensionsUrlOverrideRegistryManager(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_object,
      Profile* profile);
  virtual ~ExtensionsUrlOverrideRegistryManager();

  ExtensionsUrlOverrideRegistryManager(
      const ExtensionsUrlOverrideRegistryManager& client) = delete;
  ExtensionsUrlOverrideRegistryManager& operator=(
      const ExtensionsUrlOverrideRegistryManager& client) = delete;

  // Called by Java to destroy this object. Do not call directly in C++.
  void Destroy(JNIEnv* env);

  void OnUrlOverrideEnabled(const std::string& chrome_url_path,
                            bool incognito_enabled) override;
  void OnUrlOverrideDisabled(const std::string& chrome_url_path) override;

  jni_zero::ScopedJavaGlobalRef<jobject> j_object_;
  std::unique_ptr<ExtensionUrlOverrideStateTracker> state_tracker_;
};
}  // namespace extensions

#endif  // CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_REGISTRY_MANAGER_H_

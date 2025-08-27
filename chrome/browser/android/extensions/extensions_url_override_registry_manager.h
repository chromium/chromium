// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_REGISTRY_MANAGER_H_
#define CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_REGISTRY_MANAGER_H_

#include <jni.h>

namespace extensions {
// Listens to changes to the Native-level extensions URL registry and handles
// updates to Android classes.
class ExtensionsUrlOverrideRegistryManager {
 public:
  ExtensionsUrlOverrideRegistryManager() = default;

  ~ExtensionsUrlOverrideRegistryManager() = default;

  ExtensionsUrlOverrideRegistryManager(
      const ExtensionsUrlOverrideRegistryManager& client) = delete;
  ExtensionsUrlOverrideRegistryManager& operator=(
      const ExtensionsUrlOverrideRegistryManager& client) = delete;

  // Called by Java to destroy this object. Do not call directly in C++.
  void Destroy(JNIEnv* env);
};
}  // namespace extensions

#endif  // CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_REGISTRY_MANAGER_H_

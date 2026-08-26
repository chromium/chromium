// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_ANDROID_NATIVE_MESSAGING_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_ANDROID_NATIVE_MESSAGING_MANAGER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class Extension;
enum class UnloadedExtensionReason;

// Native C++ counterpart to Java NativeMessagingManager.
// Owned by NativeMessagingManager.java.
class NativeMessagingManager : public ExtensionRegistryObserver {
 public:
  NativeMessagingManager(JNIEnv* env,
                         const base::android::JavaRef<jobject>& j_object,
                         Profile* profile);
  ~NativeMessagingManager() override;

  NativeMessagingManager(const NativeMessagingManager&) = delete;
  NativeMessagingManager& operator=(const NativeMessagingManager&) = delete;

  // Called by Java to destroy this object. Do not call directly in C++.
  void Destroy(JNIEnv* env);

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnShutdown(ExtensionRegistry* registry) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_peer_;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_ANDROID_NATIVE_MESSAGING_MANAGER_H_

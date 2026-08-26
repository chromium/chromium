// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/android/native_messaging_manager.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from NativeMessagingManager.java.
#include "chrome/browser/extensions/api/messaging/android/jni_headers/NativeMessagingManager_jni.h"

namespace extensions {

static int64_t JNI_NativeMessagingManager_Initialize(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_object,
    Profile* profile) {
  return reinterpret_cast<intptr_t>(
      new NativeMessagingManager(env, j_object, profile));
}

NativeMessagingManager::NativeMessagingManager(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_object,
    Profile* profile)
    : java_peer_(env, j_object) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile));
}

NativeMessagingManager::~NativeMessagingManager() = default;

void NativeMessagingManager::Destroy(JNIEnv* env) {
  delete this;
}

void NativeMessagingManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NativeMessagingManager_onExtensionUnloaded(env, java_peer_,
                                                  extension->id());
}

void NativeMessagingManager::OnShutdown(ExtensionRegistry* registry) {
  extension_registry_observation_.Reset();
}

}  // namespace extensions

DEFINE_JNI(NativeMessagingManager)

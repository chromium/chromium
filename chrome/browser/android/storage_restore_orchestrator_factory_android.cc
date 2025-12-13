// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/storage_restore_orchestrator_factory_android.h"

#include <jni.h>

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/storage_loaded_data_android.h"
#include "chrome/browser/android/tab_state_storage_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from StorageRestoreOrchestratorFactory.java.
#include "chrome/browser/tab/jni_headers/StorageRestoreOrchestratorFactory_jni.h"

namespace tabs {

static jlong JNI_StorageRestoreOrchestratorFactory_Build(
    JNIEnv* env,
    Profile* profile,
    tabs::TabStripCollection* collection,
    StorageLoadedDataAndroid* loaded_data) {
  TabStateStorageService* service =
      TabStateStorageServiceFactory::GetForProfile(profile);
  StorageRestoreOrchestrator* orchestrator = new StorageRestoreOrchestrator(
      collection, service, loaded_data->GetData());
  return reinterpret_cast<intptr_t>(orchestrator);
}

// static
std::unique_ptr<StorageRestoreOrchestrator>
StorageRestoreOrchestratorFactoryAndroid::Build(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj) {
  CHECK(obj);
  StorageRestoreOrchestrator* orchestrator =
      reinterpret_cast<StorageRestoreOrchestrator*>(
          Java_StorageRestoreOrchestratorFactory_build(env, obj));
  return base::WrapUnique(orchestrator);
}

}  // namespace tabs

DEFINE_JNI(StorageRestoreOrchestratorFactory)

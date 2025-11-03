// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/storage_restore_orchestrator_android.h"

#include <jni.h>

#include <memory>

#include "chrome/browser/android/tab_state_storage_service_factory.h"
#include "chrome/browser/tab/storage_restore_orchestrator.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from StorageRestoreOrchestrator.java.
#include "chrome/browser/tab/jni_headers/StorageRestoreOrchestrator_jni.h"

namespace tabs {

StorageRestoreOrchestratorAndroid::StorageRestoreOrchestratorAndroid(
    Profile* profile,
    tabs::TabStripCollection* collection,
    StorageLoadedData* loaded_data)
    : orchestrator_(collection,
                    TabStateStorageServiceFactory::GetForProfile(profile),
                    loaded_data) {}

StorageRestoreOrchestratorAndroid::~StorageRestoreOrchestratorAndroid() =
    default;

void StorageRestoreOrchestratorAndroid::Save(JNIEnv* env) {
  orchestrator_.Save();
}

static jlong JNI_StorageRestoreOrchestrator_Init(
    JNIEnv* env,
    Profile* profile,
    tabs::TabStripCollection* collection,
    StorageLoadedDataAndroid* loaded_data) {
  StorageRestoreOrchestratorAndroid* orchestrator =
      new StorageRestoreOrchestratorAndroid(profile, collection,
                                            loaded_data->GetData());
  return reinterpret_cast<intptr_t>(orchestrator);
}

void StorageRestoreOrchestratorAndroid::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace tabs

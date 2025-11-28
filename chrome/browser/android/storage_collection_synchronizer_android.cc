// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/storage_collection_synchronizer_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/collection_storage_observer_factory_android.h"
#include "chrome/browser/android/storage_restore_orchestrator_factory_android.h"
#include "chrome/browser/android/tab_state_storage_service_factory.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from StorageCollectionSynchronizer.java.
#include "chrome/browser/tab/jni_headers/StorageCollectionSynchronizer_jni.h"

namespace tabs {

StorageCollectionSynchronizerAndroid::StorageCollectionSynchronizerAndroid(
    Profile* profile,
    tabs::TabStripCollection* collection)
    : synchronizer_(collection,
                    TabStateStorageServiceFactory::GetForProfile(profile)) {}

StorageCollectionSynchronizerAndroid::~StorageCollectionSynchronizerAndroid() =
    default;

void StorageCollectionSynchronizerAndroid::FullSave(JNIEnv* env) {
  synchronizer_.FullSave();
}

void StorageCollectionSynchronizerAndroid::ConsumeRestoreOrchestratorFactory(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_object) {
  synchronizer_.SetCollectionObserver(
      StorageRestoreOrchestratorFactoryAndroid::Build(env, j_object));
}

void StorageCollectionSynchronizerAndroid::ConsumeCollectionObserverFactory(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_object) {
  synchronizer_.SetCollectionObserver(
      CollectionStorageObserverFactoryAndroid::Build(env, j_object));
}

static jlong JNI_StorageCollectionSynchronizer_Init(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_object,
    Profile* profile,
    tabs::TabStripCollection* collection) {
  StorageCollectionSynchronizerAndroid* tracker =
      new StorageCollectionSynchronizerAndroid(profile, collection);
  return reinterpret_cast<intptr_t>(tracker);
}

void StorageCollectionSynchronizerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace tabs

DEFINE_JNI(StorageCollectionSynchronizer)

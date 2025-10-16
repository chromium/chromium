// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/collection_structure_tracker_android.h"

#include <jni.h>

#include <memory>

#include "chrome/browser/android/tab_state_storage_service_factory.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from CollectionStructureTracker.java.
#include "chrome/browser/tab/jni_headers/CollectionStructureTracker_jni.h"

namespace tabs {

CollectionStructureTrackerAndroid::CollectionStructureTrackerAndroid(
    Profile* profile,
    tabs::TabStripCollection* collection) {
  TabStateStorageService* service =
      TabStateStorageServiceFactory::GetForProfile(profile);
  tracker_ = std::make_unique<CollectionStructureTracker>(collection, service);
}

CollectionStructureTrackerAndroid::~CollectionStructureTrackerAndroid() =
    default;

void CollectionStructureTrackerAndroid::FullSave(JNIEnv* env) {
  tracker_->FullSave();
}

static jlong JNI_CollectionStructureTracker_Init(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_object,
    Profile* profile,
    tabs::TabStripCollection* collection) {
  CollectionStructureTrackerAndroid* tracker =
      new CollectionStructureTrackerAndroid(profile, collection);
  return reinterpret_cast<intptr_t>(tracker);
}

void CollectionStructureTrackerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace tabs

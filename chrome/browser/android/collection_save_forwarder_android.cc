// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/collection_save_forwarder_android.h"

#include <algorithm>
#include <memory>

#include "base/android/token_android.h"
#include "chrome/browser/android/tab_state_storage_service_factory.h"
#include "chrome/browser/tab/collection_save_forwarder.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from CollectionSaveForwarder.java.
#include "chrome/browser/tab/jni_headers/CollectionSaveForwarder_jni.h"

namespace tabs {

CollectionSaveForwarderAndroid::CollectionSaveForwarderAndroid(
    Profile* profile,
    tabs::TabStripCollection* collection) {
  TabStateStorageService* service =
      TabStateStorageServiceFactory::GetForProfile(profile);
  save_forwarder_ =
      std::make_unique<CollectionSaveForwarder>(collection, service);
}

CollectionSaveForwarderAndroid::CollectionSaveForwarderAndroid(
    std::unique_ptr<CollectionSaveForwarder> save_forwarder)
    : save_forwarder_(std::move(save_forwarder)) {}

CollectionSaveForwarderAndroid::~CollectionSaveForwarderAndroid() = default;

static jlong JNI_CollectionSaveForwarder_CreateForTabGroup(
    JNIEnv* env,
    Profile* profile,
    base::Token& tab_group_id,
    tabs::TabStripCollection* collection) {
  TabStateStorageService* service =
      TabStateStorageServiceFactory::GetForProfile(profile);

  std::unique_ptr<CollectionSaveForwarder> save_forwarder =
      CollectionSaveForwarder::CreateForTabGroupTabCollection(
          tab_groups::TabGroupId::FromRawToken(tab_group_id), collection,
          service);

  CollectionSaveForwarderAndroid* wrapper =
      new CollectionSaveForwarderAndroid(std::move(save_forwarder));
  return reinterpret_cast<intptr_t>(wrapper);
}

void CollectionSaveForwarderAndroid::Destroy(JNIEnv* env) {
  delete this;
}

void CollectionSaveForwarderAndroid::Save(JNIEnv* env) {
  save_forwarder_->Save();
}

}  // namespace tabs

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/collection_storage_observer_factory_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/tab_state_storage_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/collection_storage_observer.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from CollectionStorageObserverFactory.java.
#include "chrome/browser/tab/jni_headers/CollectionStorageObserverFactory_jni.h"

namespace tabs {

static int64_t JNI_CollectionStorageObserverFactory_Build(
    JNIEnv* env,
    Profile* profile,
    TabStripCollection* collection) {
  TabStateStorageService* service =
      TabStateStorageServiceFactory::GetForProfile(profile);
  CollectionStorageObserver* observer =
      new CollectionStorageObserver(collection, service);
  return reinterpret_cast<intptr_t>(observer);
}

// static
std::unique_ptr<CollectionStorageObserver>
CollectionStorageObserverFactoryAndroid::Build(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj) {
  CHECK(obj);
  CollectionStorageObserver* observer =
      reinterpret_cast<CollectionStorageObserver*>(
          Java_CollectionStorageObserverFactory_build(env, obj));
  return base::WrapUnique(observer);
}

}  // namespace tabs

DEFINE_JNI(CollectionStorageObserverFactory)

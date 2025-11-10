// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COLLECTION_STORAGE_OBSERVER_FACTORY_ANDROID_H_
#define CHROME_BROWSER_ANDROID_COLLECTION_STORAGE_OBSERVER_FACTORY_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/tab/collection_storage_observer.h"
#include "third_party/jni_zero/jni_zero.h"

namespace tabs {

// Android factory for CollectionStorageObserver. Used to decouple lifetimes
// between C++ and Java objects. Will not be instantiated.
class CollectionStorageObserverFactoryAndroid {
 public:
  // Builds an observer using data the associated factory object in Java.
  static std::unique_ptr<CollectionStorageObserver> Build(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& obj);
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_COLLECTION_STORAGE_OBSERVER_FACTORY_ANDROID_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_STORAGE_COLLECTION_SYNCHRONIZER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_STORAGE_COLLECTION_SYNCHRONIZER_ANDROID_H_

#include <jni.h>

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/storage_collection_synchronizer.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

// Android wrapper for StorageCollectionSynchronizer.
class StorageCollectionSynchronizerAndroid {
 public:
  StorageCollectionSynchronizerAndroid(Profile* profile,
                                       tabs::TabStripCollection* collection);
  ~StorageCollectionSynchronizerAndroid();

  StorageCollectionSynchronizerAndroid(
      const StorageCollectionSynchronizerAndroid&) = delete;
  StorageCollectionSynchronizerAndroid& operator=(
      const StorageCollectionSynchronizerAndroid&) = delete;

  void FullSave(JNIEnv* env);

  // Should only be destroyed through Java object.
  void Destroy(JNIEnv* env);

 private:
  std::unique_ptr<StorageCollectionSynchronizer> tracker_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_STORAGE_COLLECTION_SYNCHRONIZER_ANDROID_H_

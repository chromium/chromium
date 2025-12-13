// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COLLECTION_SAVE_FORWARDER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_COLLECTION_SAVE_FORWARDER_ANDROID_H_

#include <jni.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/collection_save_forwarder.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

// Android wrapper for CollectionSaveForwarder.
class CollectionSaveForwarderAndroid {
 public:
  CollectionSaveForwarderAndroid(Profile* profile,
                                 tabs::TabStripCollection* collection);
  explicit CollectionSaveForwarderAndroid(
      CollectionSaveForwarder save_forwarder);
  ~CollectionSaveForwarderAndroid();

  CollectionSaveForwarderAndroid(const CollectionSaveForwarderAndroid&) =
      delete;
  CollectionSaveForwarderAndroid& operator=(
      const CollectionSaveForwarderAndroid&) = delete;

  // Should only be destroyed through Java object.
  void Destroy(JNIEnv* env);

  void SavePayload(JNIEnv* env);

 private:
  CollectionSaveForwarder save_forwarder_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_COLLECTION_SAVE_FORWARDER_ANDROID_H_

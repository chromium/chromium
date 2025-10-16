// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COLLECTION_STRUCTURE_TRACKER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_COLLECTION_STRUCTURE_TRACKER_ANDROID_H_

#include <jni.h>

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/collection_structure_tracker.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

// Android wrapper for CollectionStructureTracker.
class CollectionStructureTrackerAndroid {
 public:
  CollectionStructureTrackerAndroid(Profile* profile,
                                    tabs::TabStripCollection* collection);
  ~CollectionStructureTrackerAndroid();

  CollectionStructureTrackerAndroid(const CollectionStructureTrackerAndroid&) =
      delete;
  CollectionStructureTrackerAndroid& operator=(
      const CollectionStructureTrackerAndroid&) = delete;

  void FullSave(JNIEnv* env);

  // Should only be destroyed through Java object.
  void Destroy(JNIEnv* env);

 private:
  std::unique_ptr<CollectionStructureTracker> tracker_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_COLLECTION_STRUCTURE_TRACKER_ANDROID_H_

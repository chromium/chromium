// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_STORAGE_RESTORE_ORCHESTRATOR_ANDROID_H_
#define CHROME_BROWSER_ANDROID_STORAGE_RESTORE_ORCHESTRATOR_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/storage_loaded_data_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/storage_restore_orchestrator.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

// Android wrapper for StorageRestoreOrchestrator.
class StorageRestoreOrchestratorAndroid {
 public:
  StorageRestoreOrchestratorAndroid(Profile* profile,
                                    tabs::TabStripCollection* collection,
                                    StorageLoadedData* loaded_data);
  ~StorageRestoreOrchestratorAndroid();

  void Save(JNIEnv* env);
  void Destroy(JNIEnv* env);

 private:
  StorageRestoreOrchestrator orchestrator_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_STORAGE_RESTORE_ORCHESTRATOR_ANDROID_H_

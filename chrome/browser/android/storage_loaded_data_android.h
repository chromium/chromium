// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_STORAGE_LOADED_DATA_ANDROID_H_
#define CHROME_BROWSER_ANDROID_STORAGE_LOADED_DATA_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/tab/storage_loaded_data.h"

namespace tabs {

// Creation methods for the android counterpart of StorageLoadedData.
class StorageLoadedDataAndroid {
 public:
  // Should not be instantiated.
  StorageLoadedDataAndroid() = delete;
  ~StorageLoadedDataAndroid() = delete;

  // Create the android counterpart.
  static base::android::ScopedJavaLocalRef<jobject> CreateLoadedData(
      JNIEnv* env,
      StorageLoadedData loaded_data);
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_STORAGE_LOADED_DATA_ANDROID_H_

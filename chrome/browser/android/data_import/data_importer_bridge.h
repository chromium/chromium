// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATA_IMPORT_DATA_IMPORTER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_DATA_IMPORT_DATA_IMPORTER_BRIDGE_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/user_data_importer/content/stable_portability_data_importer.h"

class Profile;

class DataImporterBridge {
 public:
  explicit DataImporterBridge(Profile* profile);

  DataImporterBridge(const DataImporterBridge&) = delete;
  DataImporterBridge& operator=(const DataImporterBridge&) = delete;
  ~DataImporterBridge();

  void Destroy(JNIEnv* env);

  void ImportBookmarks(JNIEnv* env,
                       jint owned_fd,
                       const base::android::JavaRef<jobject>& j_callback);

  void ImportReadingList(JNIEnv* env,
                         jint owned_fd,
                         const base::android::JavaRef<jobject>& j_callback);

  void ImportHistory(JNIEnv* env,
                     jint owned_fd,
                     const base::android::JavaRef<jobject>& j_callback);

 private:
  void ImportBookmarksDone(base::android::ScopedJavaGlobalRef<jobject> callback,
                           int count);

  void ImportReadingListDone(
      base::android::ScopedJavaGlobalRef<jobject> callback,
      int count);

  void ImportHistoryDone(base::android::ScopedJavaGlobalRef<jobject> callback,
                         int count);

  raw_ptr<Profile> profile_;

  std::unique_ptr<user_data_importer::StablePortabilityDataImporter> importer_;

  base::WeakPtrFactory<DataImporterBridge> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_DATA_IMPORT_DATA_IMPORTER_BRIDGE_H_

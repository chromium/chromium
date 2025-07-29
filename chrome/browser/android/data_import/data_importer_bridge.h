// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATA_IMPORT_DATA_IMPORTER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_DATA_IMPORT_DATA_IMPORTER_BRIDGE_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "components/user_data_importer/content/stable_portability_data_importer.h"

class Profile;

class DataImporterBridge {
 public:
  explicit DataImporterBridge(Profile* profile);

  DataImporterBridge(const DataImporterBridge&) = delete;
  DataImporterBridge& operator=(const DataImporterBridge&) = delete;
  ~DataImporterBridge();

  void Destroy(JNIEnv* env);

  void ImportBookmarks(JNIEnv* env, jint owned_fd);

  void ImportReadingList(JNIEnv* env, jint owned_fd);

 private:
  raw_ptr<Profile> profile_;

  std::unique_ptr<user_data_importer::StablePortabilityDataImporter> importer_;
};

#endif  // CHROME_BROWSER_ANDROID_DATA_IMPORT_DATA_IMPORTER_BRIDGE_H_

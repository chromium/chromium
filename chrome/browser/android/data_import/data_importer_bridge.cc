// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_import/data_importer_bridge.h"

#include <memory>
#include <utility>

#include "base/android/callback_android.h"
#include "base/files/file.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/user_data_importer/content/content_bookmark_parser.h"
#include "components/user_data_importer/content/stable_portability_data_importer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/DataImporterBridge_jni.h"

DataImporterBridge::DataImporterBridge(Profile* profile) : profile_(profile) {
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  auto* bookmark_model = BookmarkModelFactory::GetForBrowserContext(profile_);
  auto* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile_);
  importer_ =
      std::make_unique<user_data_importer::StablePortabilityDataImporter>(
          history_service, bookmark_model, reading_list_model,
          std::make_unique<user_data_importer::ContentBookmarkParser>());
}

DataImporterBridge::~DataImporterBridge() = default;

void DataImporterBridge::Destroy(JNIEnv* env) {
  delete this;
}

void DataImporterBridge::ImportBookmarks(
    JNIEnv* env,
    jint owned_fd,
    const base::android::JavaRef<jobject>& j_callback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(j_callback);
  base::File file(owned_fd, base::File::FLAG_OPEN | base::File::FLAG_READ);
  importer_->ImportBookmarks(
      std::move(file),
      base::BindOnce(&DataImporterBridge::ImportBookmarksDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void DataImporterBridge::ImportReadingList(
    JNIEnv* env,
    jint owned_fd,
    const base::android::JavaRef<jobject>& j_callback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(j_callback);
  base::File file(owned_fd, base::File::FLAG_OPEN | base::File::FLAG_READ);
  importer_->ImportReadingList(
      std::move(file),
      base::BindOnce(&DataImporterBridge::ImportReadingListDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void DataImporterBridge::ImportHistory(
    JNIEnv* env,
    jint owned_fd,
    const base::android::JavaRef<jobject>& j_callback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(j_callback);
  base::File file(owned_fd, base::File::FLAG_OPEN | base::File::FLAG_READ);
  importer_->ImportHistory(
      std::move(file),
      base::BindOnce(&DataImporterBridge::ImportHistoryDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void DataImporterBridge::ImportBookmarksDone(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    int count) {
  base::android::RunIntCallbackAndroid(callback, count);
}

void DataImporterBridge::ImportReadingListDone(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    int count) {
  base::android::RunIntCallbackAndroid(callback, count);
}

void DataImporterBridge::ImportHistoryDone(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    int count) {
  base::android::RunIntCallbackAndroid(callback, count);
}

static jlong JNI_DataImporterBridge_Init(JNIEnv* env, Profile* profile) {
  DataImporterBridge* bridge = new DataImporterBridge(profile);
  return reinterpret_cast<intptr_t>(bridge);
}

DEFINE_JNI(DataImporterBridge)

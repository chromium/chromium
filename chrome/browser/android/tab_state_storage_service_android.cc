// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_state_storage_service_android.h"

#include <memory>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_string.h"
#include "base/android/token_android.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/token.h"
#include "chrome/browser/android/storage_loaded_data_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_group_collection_data_android.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "chrome/browser/tab/tab_state_storage_backend.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "chrome/browser/tab/tab_storage_util.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "components/tabs/public/direct_child_walker.h"
#include "components/tabs/public/tab_strip_collection.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/TabStateStorageService_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace tabs {

namespace {

void RunJavaCallbackLoadAll(JNIEnv* env,
                            const JavaRef<jobject>& j_loaded_data_callback,
                            std::unique_ptr<StorageLoadedData> loaded_data) {
  StorageLoadedDataAndroid* data_android =
      new StorageLoadedDataAndroid(env, std::move(loaded_data));
  base::android::RunObjectCallbackAndroid(j_loaded_data_callback,
                                          data_android->GetJavaObject());
}

void RunJavaCallbackCountTabsForWindow(JNIEnv* env,
                                       const JavaRef<jobject>& j_count_callback,
                                       int count) {
  base::android::RunIntCallbackAndroid(j_count_callback, count);
}

// Recursively crawls the entire tree and retrieves storage ids for all nodes.
class CollectionStorageIdCrawler : public DirectChildWalker::Processor {
 public:
  explicit CollectionStorageIdCrawler(StorageIdMapping& mapping,
                                      std::vector<StorageId>& ids)
      : mapping_(mapping), ids_(ids) {}

  void ProcessTab(const TabInterface* tab) override {
    StorageId id = mapping_.get().GetStorageId(tab);
    ids_->push_back(id);
  }

  void ProcessCollection(const TabCollection* collection) override {
    StorageId id = mapping_.get().GetStorageId(collection);
    ids_->push_back(id);

    DirectChildWalker walker(collection, this);
    walker.Walk();
  }

 private:
  raw_ref<StorageIdMapping> mapping_;
  raw_ref<std::vector<StorageId>> ids_;
};

}  // namespace

using ScopedBatchAndroid = TabStateStorageServiceAndroid::ScopedBatchAndroid;

ScopedBatchAndroid::ScopedBatchAndroid(
    TabStateStorageService::ScopedBatch batch)
    : batch(std::move(batch)) {}

ScopedBatchAndroid::~ScopedBatchAndroid() = default;

static void JNI_TabStateStorageService_CommitBatch(JNIEnv* env,
                                                   int64_t batch_android_ptr) {
  delete reinterpret_cast<ScopedBatchAndroid*>(batch_android_ptr);
}

TabStateStorageServiceAndroid::TabStateStorageServiceAndroid(
    TabStateStorageService* tab_state_storage_service)
    : tab_state_storage_service_(tab_state_storage_service) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(Java_TabStateStorageService_create(
      env, reinterpret_cast<intptr_t>(this)));
}

TabStateStorageServiceAndroid::~TabStateStorageServiceAndroid() = default;

void TabStateStorageServiceAndroid::BoostPriority(JNIEnv* env) {
  tab_state_storage_service_->BoostPriority();
}

void TabStateStorageServiceAndroid::Save(JNIEnv* env, TabAndroid* tab) {
  tab_state_storage_service_->Save(tab);
}

void TabStateStorageServiceAndroid::LoadAllData(
    JNIEnv* env,
    const std::string& window_tag,
    bool is_off_the_record,
    const jni_zero::JavaRef<jobject>& j_loaded_data_callback) {
  auto load_data_callback = base::BindOnce(
      &RunJavaCallbackLoadAll, env,
      jni_zero::ScopedJavaGlobalRef<jobject>(j_loaded_data_callback));
  tab_state_storage_service_->LoadAllNodes(window_tag, is_off_the_record,
                                           std::move(load_data_callback));
}

void TabStateStorageServiceAndroid::CountTabsForWindow(
    JNIEnv* env,
    const std::string& window_tag,
    bool is_off_the_record,
    const jni_zero::JavaRef<jobject>& j_callback) {
  auto count_callback =
      base::BindOnce(&RunJavaCallbackCountTabsForWindow, env,
                     jni_zero::ScopedJavaGlobalRef<jobject>(j_callback));
  tab_state_storage_service_->CountTabsForWindow(window_tag, is_off_the_record,
                                                 std::move(count_callback));
}

void TabStateStorageServiceAndroid::ClearState(JNIEnv* env) {
  auto scoped_batch = tab_state_storage_service_->CreateScopedBatch();
  tab_state_storage_service_->ClearAllWindows();
  tab_state_storage_service_->ClearAllDivergenceWindows();
}

void TabStateStorageServiceAndroid::ClearWindow(JNIEnv* env,
                                                const std::string& window_tag) {
  auto scoped_batch = tab_state_storage_service_->CreateScopedBatch();
  tab_state_storage_service_->ClearWindow(window_tag);
  tab_state_storage_service_->ClearDivergenceWindow(window_tag);
}

void TabStateStorageServiceAndroid::ClearWindowWithOtrStatus(
    JNIEnv* env,
    const std::string& window_tag,
    bool is_off_the_record) {
  std::vector<StorageId> ids;
  auto scoped_batch = tab_state_storage_service_->CreateScopedBatch();
  tab_state_storage_service_->ClearNodesForWindowExcept(window_tag,
                                                        is_off_the_record, ids);
  tab_state_storage_service_->ClearDivergentNodesForWindow(window_tag,
                                                           is_off_the_record);
}

void TabStateStorageServiceAndroid::ClearUnusedNodesForWindow(
    JNIEnv* env,
    const std::string& window_tag,
    bool is_off_the_record,
    const TabStripCollection* collection) {
  std::vector<StorageId> ids;

  if (collection) {
    ids.push_back(tab_state_storage_service_->GetStorageId(collection));
    CollectionStorageIdCrawler crawler(*tab_state_storage_service_, ids);

    DirectChildWalker walker(collection, &crawler);
    walker.Walk();
  }

  // We do not need to clear the divergence window since divergent nodes will
  // never remain when this method is called.
  tab_state_storage_service_->ClearNodesForWindowExcept(window_tag,
                                                        is_off_the_record, ids);
}

void TabStateStorageServiceAndroid::PrintAll(JNIEnv* env) {
#if defined(NDEBUG)
  tab_state_storage_service_->PrintAll();
#endif
}

int64_t TabStateStorageServiceAndroid::CreateBatch(JNIEnv* env) {
  return reinterpret_cast<int64_t>(
      new ScopedBatchAndroid(tab_state_storage_service_->CreateScopedBatch()));
}

void TabStateStorageServiceAndroid::SetKey(JNIEnv* env,
                                           const std::string& window_tag,
                                           std::vector<uint8_t> key) {
  tab_state_storage_service_->SetKey(window_tag, std::move(key));
}

void TabStateStorageServiceAndroid::RemoveKey(JNIEnv* env,
                                              const std::string& window_tag) {
  tab_state_storage_service_->RemoveKey(window_tag);
}

ScopedJavaLocalRef<jbyteArray> TabStateStorageServiceAndroid::GenerateKey(
    JNIEnv* env,
    const std::string& window_tag) {
  auto key = tab_state_storage_service_->GenerateKey(window_tag);
  return base::android::ToJavaByteArray(env, key);
}

ScopedJavaLocalRef<jobject> TabStateStorageServiceAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

// This function is declared in tab_state_storage_service.h and
// should be linked in to any binary using
// TabStateStorageService::GetJavaObject.
// static
ScopedJavaLocalRef<jobject> TabStateStorageService::GetJavaObject(
    TabStateStorageService* tab_state_storage_service) {
  TabStateStorageServiceAndroid* service_android =
      static_cast<TabStateStorageServiceAndroid*>(
          tab_state_storage_service->GetUserData(
              kTabStateStorageServiceAndroidKey));
  if (!service_android) {
    service_android =
        new TabStateStorageServiceAndroid(tab_state_storage_service);
    tab_state_storage_service->SetUserData(kTabStateStorageServiceAndroidKey,
                                           base::WrapUnique(service_android));
  }
  return service_android->GetJavaObject();
}

}  // namespace tabs

DEFINE_JNI(TabStateStorageService)

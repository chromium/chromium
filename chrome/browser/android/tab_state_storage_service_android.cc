// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_state_storage_service_android.h"

#include <memory>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_callback.h"
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
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "chrome/browser/tab/tab_state_storage_backend.h"
#include "chrome/browser/tab/tab_state_storage_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/TabStateStorageService_jni.h"

namespace tabs {

namespace {

void RunJavaCallbackLoadAll(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_loaded_data_callback,
    std::unique_ptr<StorageLoadedData> loaded_data) {
  StorageLoadedDataAndroid* data_android =
      new StorageLoadedDataAndroid(env, std::move(loaded_data));
  base::android::RunObjectCallbackAndroid(j_loaded_data_callback,
                                          data_android->GetJavaObject());
}

}  // namespace

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
    const jni_zero::JavaParamRef<jobject>& j_loaded_data_callback) {
  auto load_data_callback = base::BindOnce(
      &RunJavaCallbackLoadAll, env,
      jni_zero::ScopedJavaGlobalRef<jobject>(j_loaded_data_callback));
  tab_state_storage_service_->LoadAllNodes(window_tag, is_off_the_record,
                                           std::move(load_data_callback));
}

void TabStateStorageServiceAndroid::ClearState(JNIEnv* env) {
  tab_state_storage_service_->ClearState();
}

void TabStateStorageServiceAndroid::ClearWindow(JNIEnv* env,
                                                const std::string& window_tag) {
  tab_state_storage_service_->ClearWindow(window_tag);
}

base::android::ScopedJavaLocalRef<jobject>
TabStateStorageServiceAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

// This function is declared in tab_state_storage_service.h and
// should be linked in to any binary using
// TabStateStorageService::GetJavaObject.
// static
base::android::ScopedJavaLocalRef<jobject>
TabStateStorageService::GetJavaObject(
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

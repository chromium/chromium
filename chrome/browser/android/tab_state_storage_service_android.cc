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
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/token.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/tab_state_storage_backend.h"
#include "chrome/browser/tab/tab_state_storage_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/TabStateStorageService_jni.h"

namespace tabs {

namespace {

base::OnceCallback<void(TabAndroid*)> WrapCallbackForJni(
    TabStateStorageService::OnTabInterfaceCreation&& callback) {
  return base::BindOnce(
      [](TabStateStorageService::OnTabInterfaceCreation inner_cb,
         TabAndroid* tab) { std::move(inner_cb).Run(tab); },
      std::move(callback));
}

void RunJavaCallbackLoadAllTabs(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_callback,
    std::vector<TabStateStorageService::LoadedTabState> loaded_tabs) {
  std::vector<base::android::ScopedJavaLocalRef<jobject>>
      j_loaded_tab_state_vector;
  for (auto& loaded_tab : loaded_tabs) {
    tabs_pb::TabState& tab_state = loaded_tab.first;
    base::android::ScopedJavaLocalRef<jobject> j_web_contents_state_buffer;
    long j_web_contents_state_string_pointer = 0;
    if (tab_state.has_web_contents_state_bytes()) {
      std::string* web_contents_state_bytes_ptr =
          tab_state.release_web_contents_state_bytes();
      j_web_contents_state_buffer =
          base::android::ScopedJavaLocalRef<jobject>::Adopt(
              env, env->NewDirectByteBuffer(
                       static_cast<void*>(web_contents_state_bytes_ptr->data()),
                       web_contents_state_bytes_ptr->size()));
      j_web_contents_state_string_pointer =
          reinterpret_cast<long>(web_contents_state_bytes_ptr);
    }

    base::Token tab_group_token(tab_state.tab_group_id_high(),
                                tab_state.tab_group_id_low());
    base::android::ScopedJavaLocalRef<jobject> j_tab_group_id;
    if (!tab_group_token.is_zero()) {
      j_tab_group_id =
          base::android::TokenAndroid::Create(env, tab_group_token);
    }

    base::android::ScopedJavaLocalRef<jobject> j_on_tab_created_callback =
        base::android::ToJniCallback(
            env, WrapCallbackForJni(std::move(loaded_tab.second)));

    base::android::ScopedJavaLocalRef<jobject> j_tab_state =
        Java_TabStateStorageService_createTabState(
            env, tab_state.parent_id(), tab_state.root_id(),
            tab_state.timestamp_millis(), j_web_contents_state_buffer,
            tab_state.web_contents_state_version(),
            j_web_contents_state_string_pointer, tab_state.opener_app_id(),
            tab_state.theme_color(), tab_state.launch_type_at_creation(),
            tab_state.user_agent(),
            tab_state.last_navigation_committed_timestamp_millis(),
            j_tab_group_id, tab_state.tab_has_sensitive_content(),
            tab_state.is_pinned());

    base::android::ScopedJavaLocalRef<jobject> j_loaded_tab_state =
        Java_TabStateStorageService_createLoadedTabState(
            env, tab_state.tab_id(), j_tab_state, j_on_tab_created_callback);
    j_loaded_tab_state_vector.push_back(j_loaded_tab_state);
  }

  base::android::ScopedJavaLocalRef<jclass> type = base::android::GetClass(
      env,
      "org/chromium/chrome/browser/tab/TabStateStorageService$LoadedTabState");
  base::android::ScopedJavaLocalRef<jobjectArray> j_loaded_tab_state_array =
      base::android::ToTypedJavaArrayOfObjects(env, j_loaded_tab_state_vector,
                                               type.obj());
  base::android::RunObjectCallbackAndroid(j_callback, j_loaded_tab_state_array);
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

void TabStateStorageServiceAndroid::Save(JNIEnv* env, TabAndroid* tab) {
  tab_state_storage_service_->Save(tab);
}

void TabStateStorageServiceAndroid::LoadAllTabs(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& j_callback) {
  auto load_all_tabs_callback =
      base::BindOnce(&RunJavaCallbackLoadAllTabs, env,
                     jni_zero::ScopedJavaGlobalRef<jobject>(j_callback));
  tab_state_storage_service_->LoadAllTabs(std::move(load_all_tabs_callback));
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

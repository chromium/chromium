// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/request_coordinator_bridge.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/RequestCoordinatorBridge_jni.h"
#include "chrome/android/chrome_jni_headers/SavePageRequest_jni.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/core/background/request_coordinator.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace offline_pages {
namespace android {

namespace {

void OnGetAllRequestsDone(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    std::vector<std::unique_ptr<SavePageRequest>> all_requests) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> j_result_obj =
      CreateJavaSavePageRequests(env, std::move(all_requests));
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_result_obj);
}

UpdateRequestResult ToUpdateRequestResult(ItemActionStatus status) {
  switch (status) {
    case ItemActionStatus::SUCCESS:
      return UpdateRequestResult::SUCCESS;
    case ItemActionStatus::NOT_FOUND:
      return UpdateRequestResult::REQUEST_DOES_NOT_EXIST;
    case ItemActionStatus::STORE_ERROR:
      return UpdateRequestResult::STORE_FAILURE;
    case ItemActionStatus::ALREADY_EXISTS:
      NOTREACHED();
  }
  return UpdateRequestResult::STORE_FAILURE;
}

void OnRemoveRequestsDone(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                          const MultipleItemStatuses& removed_request_results) {
  JNIEnv* env = base::android::AttachCurrentThread();

  std::vector<int> update_request_results;
  std::vector<int64_t> update_request_ids;

  for (const std::pair<int64_t, ItemActionStatus>& remove_result :
       removed_request_results) {
    update_request_ids.emplace_back(std::get<0>(remove_result));
    update_request_results.emplace_back(
        static_cast<int>(ToUpdateRequestResult(std::get<1>(remove_result))));
  }

  ScopedJavaLocalRef<jlongArray> j_result_ids =
      base::android::ToJavaLongArray(env, update_request_ids);
  ScopedJavaLocalRef<jintArray> j_result_codes =
      base::android::ToJavaIntArray(env, update_request_results);

  Java_RequestsRemovedCallback_onResult(env, j_callback_obj, j_result_ids,
                                        j_result_codes);
}

void SavePageLaterCallback(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                           AddRequestResult value) {
  base::android::RunIntCallbackAndroid(j_callback_obj, static_cast<int>(value));
}

RequestCoordinator* GetRequestCoordinator(
    const JavaParamRef<jobject>& j_profile) {
  content::BrowserContext* context =
      ProfileAndroid::FromProfileAndroid(j_profile);
  return offline_pages::RequestCoordinatorFactory::GetInstance()
      ->GetForBrowserContext(context);
}

}  // namespace

ScopedJavaLocalRef<jobjectArray> CreateJavaSavePageRequests(
    JNIEnv* env,
    const std::vector<std::unique_ptr<SavePageRequest>>& requests) {
  ScopedJavaLocalRef<jclass> save_page_request_clazz = base::android::GetClass(
      env, "org/chromium/chrome/browser/offlinepages/SavePageRequest");
  jobjectArray joa = env->NewObjectArray(
      requests.size(), save_page_request_clazz.obj(), nullptr);
  base::android::CheckException(env);

  for (size_t i = 0; i < requests.size(); ++i) {
    const SavePageRequest& request = *(requests[i]);
    ScopedJavaLocalRef<jstring> name_space =
        ConvertUTF8ToJavaString(env, request.client_id().name_space);
    ScopedJavaLocalRef<jstring> id =
        ConvertUTF8ToJavaString(env, request.client_id().id);
    ScopedJavaLocalRef<jstring> url =
        ConvertUTF8ToJavaString(env, request.url().spec());
    ScopedJavaLocalRef<jstring> origin =
        ConvertUTF8ToJavaString(env, request.request_origin());

    ScopedJavaLocalRef<jobject> j_save_page_request =
        Java_SavePageRequest_create(
            env, static_cast<int>(request.request_state()),
            request.request_id(), url, name_space, id, origin,
            static_cast<int>(request.auto_fetch_notification_state()));
    env->SetObjectArrayElement(joa, i, j_save_page_request.obj());
  }

  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

JNI_EXPORT void JNI_RequestCoordinatorBridge_SavePageLater(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_callback_obj,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_client_id,
    const JavaParamRef<jstring>& j_origin,
    jboolean user_requested) {
  DCHECK(j_callback_obj);

  offline_pages::ClientId client_id;
  client_id.name_space = ConvertJavaStringToUTF8(env, j_namespace);
  client_id.id = ConvertJavaStringToUTF8(env, j_client_id);

  RequestCoordinator* coordinator = GetRequestCoordinator(j_profile);

  if (!coordinator) {
    // Callback with null to signal that results are unavailable.
    base::android::RunObjectCallbackAndroid(j_callback_obj, JavaRef<jobject>());
    return;
  }

  RequestCoordinator::SavePageLaterParams params;
  params.url = GURL(ConvertJavaStringToUTF8(env, j_url));
  params.client_id = client_id;
  params.user_requested = static_cast<bool>(user_requested);
  params.availability =
      RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER;
  params.request_origin = ConvertJavaStringToUTF8(env, j_origin);

  coordinator->SavePageLater(
      params, base::BindOnce(&SavePageLaterCallback,
                             ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

JNI_EXPORT void JNI_RequestCoordinatorBridge_GetRequestsInQueue(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  RequestCoordinator* coordinator = GetRequestCoordinator(j_profile);

  if (!coordinator) {
    // Callback with null to signal that results are unavailable.
    const JavaParamRef<jobject> empty_result(nullptr);
    base::android::RunObjectCallbackAndroid(j_callback_obj, empty_result);
    return;
  }

  coordinator->GetAllRequests(
      base::BindOnce(&OnGetAllRequestsDone, j_callback_ref));
}

JNI_EXPORT void JNI_RequestCoordinatorBridge_RemoveRequestsFromQueue(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jlongArray>& j_request_ids_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  std::vector<int64_t> request_ids;
  base::android::JavaLongArrayToInt64Vector(env, j_request_ids_array,
                                            &request_ids);
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  RequestCoordinator* coordinator = GetRequestCoordinator(j_profile);

  if (!coordinator) {
    // Callback with null to signal that results are unavailable.
    const JavaParamRef<jobject> empty_result(nullptr);
    base::android::RunObjectCallbackAndroid(j_callback_obj, empty_result);
    return;
  }

  coordinator->RemoveRequests(
      request_ids, base::BindOnce(&OnRemoveRequestsDone, j_callback_ref));
}

}  // namespace android
}  // namespace offline_pages

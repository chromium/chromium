// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/offline_page_bridge.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/offline_pages/prefetch/prefetched_pages_notifier.h"
#include "chrome/browser/offline_pages/recent_tab_helper.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/core/archive_validator.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "jni/OfflinePageBridge_jni.h"
#include "jni/SavePageRequest_jni.h"
#include "net/base/filename_util.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace offline_pages {
namespace android {

namespace {

const char kOfflinePageBridgeKey[] = "offline-page-bridge";

void JNI_SavePageRequest_ToJavaOfflinePageList(
    JNIEnv* env,
    const JavaRef<jobject>& j_result_obj,
    const std::vector<OfflinePageItem>& offline_pages) {
  for (const OfflinePageItem& offline_page : offline_pages) {
    Java_OfflinePageBridge_createOfflinePageAndAddToList(
        env, j_result_obj,
        ConvertUTF8ToJavaString(env, offline_page.url.spec()),
        offline_page.offline_id,
        ConvertUTF8ToJavaString(env, offline_page.client_id.name_space),
        ConvertUTF8ToJavaString(env, offline_page.client_id.id),
        ConvertUTF16ToJavaString(env, offline_page.title),
        ConvertUTF8ToJavaString(env, offline_page.file_path.value()),
        offline_page.file_size, offline_page.creation_time.ToJavaTime(),
        offline_page.access_count, offline_page.last_access_time.ToJavaTime(),
        ConvertUTF8ToJavaString(env, offline_page.request_origin));
  }
}

ScopedJavaLocalRef<jobject> JNI_SavePageRequest_ToJavaOfflinePageItem(
    JNIEnv* env,
    const OfflinePageItem& offline_page) {
  return Java_OfflinePageBridge_createOfflinePageItem(
      env, ConvertUTF8ToJavaString(env, offline_page.url.spec()),
      offline_page.offline_id,
      ConvertUTF8ToJavaString(env, offline_page.client_id.name_space),
      ConvertUTF8ToJavaString(env, offline_page.client_id.id),
      ConvertUTF16ToJavaString(env, offline_page.title),
      ConvertUTF8ToJavaString(env, offline_page.file_path.value()),
      offline_page.file_size, offline_page.creation_time.ToJavaTime(),
      offline_page.access_count, offline_page.last_access_time.ToJavaTime(),
      ConvertUTF8ToJavaString(env, offline_page.request_origin));
}

ScopedJavaLocalRef<jobject> JNI_SavePageRequest_ToJavaDeletedPageInfo(
    JNIEnv* env,
    const OfflinePageModel::DeletedPageInfo& deleted_page) {
  return Java_OfflinePageBridge_createDeletedPageInfo(
      env, deleted_page.offline_id,
      ConvertUTF8ToJavaString(env, deleted_page.client_id.name_space),
      ConvertUTF8ToJavaString(env, deleted_page.client_id.id),
      ConvertUTF8ToJavaString(env, deleted_page.request_origin));
}

void MultipleOfflinePageItemCallback(
    const ScopedJavaGlobalRef<jobject>& j_result_obj,
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageModel::MultipleOfflinePageItemResult& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  JNI_SavePageRequest_ToJavaOfflinePageList(env, j_result_obj, result);
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_result_obj);
}

void SavePageCallback(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                      const GURL& url,
                      OfflinePageModel::SavePageResult result,
                      int64_t offline_id) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_SavePageCallback_onSavePageDone(
      env, j_callback_obj, static_cast<int>(result),
      ConvertUTF8ToJavaString(env, url.spec()), offline_id);
}

void DeletePageCallback(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                        OfflinePageModel::DeletePageResult result) {
  base::android::RunIntCallbackAndroid(j_callback_obj,
                                       static_cast<int>(result));
}

void SelectPageCallback(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                        const std::vector<OfflinePageItem>& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_result;

  if (!result.empty())
    j_result = JNI_SavePageRequest_ToJavaOfflinePageItem(env, result.front());
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_result);
}

void SingleOfflinePageItemCallback(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageItem* result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_result;

  if (result)
    j_result = JNI_SavePageRequest_ToJavaOfflinePageItem(env, *result);
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_result);
}

void CheckForNewOfflineContentCallback(
    const base::Time& pages_created_after,
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageModel::MultipleOfflinePageItemResult& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::string16 relevant_host =
      ExtractRelevantHostFromOfflinePageItemList(pages_created_after, result);
  ScopedJavaLocalRef<jstring> j_result =
      base::android::ConvertUTF16ToJavaString(env, relevant_host);

  base::android::RunObjectCallbackAndroid(j_callback_obj, j_result);
}

void RunLoadUrlParamsCallbackAndroid(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const GURL& url,
    const OfflinePageHeader& offline_page_header) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> loadUrlParams =
      Java_OfflinePageBridge_createLoadUrlParams(
          env, ConvertUTF8ToJavaString(env, url.spec()),
          ConvertUTF8ToJavaString(env,
                                  offline_page_header.GetHeaderKeyString()),
          ConvertUTF8ToJavaString(env,
                                  offline_page_header.GetHeaderValueString()));
  base::android::RunObjectCallbackAndroid(j_callback_obj, loadUrlParams);
}

void ValidateFileCallback(
    offline_items_collection::LaunchLocation launch_location,
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    int64_t offline_id,
    const GURL& url,
    const base::FilePath& file_path,
    bool is_trusted) {
  // If trusted, the launch url will be the http/https url of the offline
  // page. Otherwise, the launch url will be the file URL pointing to the
  // archive file of the offline page.
  GURL launch_url;
  if (is_trusted)
    launch_url = url;
  else
    launch_url = net::FilePathToFileURL(file_path);
  offline_pages::OfflinePageHeader offline_header;
  switch (launch_location) {
    case offline_items_collection::LaunchLocation::NOTIFICATION:
      offline_header.reason =
          offline_pages::OfflinePageHeader::Reason::NOTIFICATION;
      break;
    case offline_items_collection::LaunchLocation::PROGRESS_BAR:
      offline_header.reason =
          offline_pages::OfflinePageHeader::Reason::PROGRESS_BAR;
      break;
    case offline_items_collection::LaunchLocation::SUGGESTION:
      offline_header.reason =
          offline_pages::OfflinePageHeader::Reason::SUGGESTION;
      break;
    case offline_items_collection::LaunchLocation::DOWNLOAD_HOME:
      offline_header.reason =
          offline_pages::OfflinePageHeader::Reason::DOWNLOAD;
      break;
    case offline_items_collection::LaunchLocation::NET_ERROR_SUGGESTION:
      offline_header.reason =
          offline_pages::OfflinePageHeader::Reason::NET_ERROR_SUGGESTION;
      break;
    case offline_items_collection::LaunchLocation::DOWNLOAD_SHELF:
      NOTREACHED();
      break;
  }
  offline_header.need_to_persist = true;
  offline_header.id = base::Int64ToString(offline_id);

  RunLoadUrlParamsCallbackAndroid(j_callback_obj, launch_url, offline_header);
}

ScopedJavaLocalRef<jobjectArray> JNI_SavePageRequest_CreateJavaSavePageRequests(
    JNIEnv* env,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  ScopedJavaLocalRef<jclass> save_page_request_clazz = base::android::GetClass(
      env, "org/chromium/chrome/browser/offlinepages/SavePageRequest");
  jobjectArray joa = env->NewObjectArray(
      requests.size(), save_page_request_clazz.obj(), nullptr);
  base::android::CheckException(env);

  for (size_t i = 0; i < requests.size(); ++i) {
    SavePageRequest request = *(requests[i]);
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
            request.request_id(), url, name_space, id, origin);
    env->SetObjectArrayElement(joa, i, j_save_page_request.obj());
  }

  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

void OnGetAllRequestsDone(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    std::vector<std::unique_ptr<SavePageRequest>> all_requests) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> j_result_obj =
      JNI_SavePageRequest_CreateJavaSavePageRequests(env,
                                                     std::move(all_requests));
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
    default:
      NOTREACHED();
  }
  return UpdateRequestResult::STORE_FAILURE;
}

void OnRemoveRequestsDone(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                          const MultipleItemStatuses& removed_request_results) {
  JNIEnv* env = base::android::AttachCurrentThread();

  std::vector<int> update_request_results;
  std::vector<int64_t> update_request_ids;

  for (std::pair<int64_t, ItemActionStatus> remove_result :
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

void PublishPageDone(
    const ScopedJavaGlobalRef<jobject>& j_published_callback_obj,
    const base::FilePath& file_path,
    SavePageResult result) {
  base::FilePath file_path_or_empty;
  if (result != SavePageResult::SUCCESS)
    file_path_or_empty = file_path;

  UMA_HISTOGRAM_ENUMERATION("OfflinePages.Sharing.PublishInternalPageResult",
                            result);

  base::android::RunStringCallbackAndroid(j_published_callback_obj,
                                          file_path.value());
}

}  // namespace

static jboolean JNI_OfflinePageBridge_IsOfflineBookmarksEnabled(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  return offline_pages::IsOfflineBookmarksEnabled();
}

static jboolean JNI_OfflinePageBridge_IsPageSharingEnabled(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  return offline_pages::IsOfflinePagesSharingEnabled();
}

static jboolean JNI_OfflinePageBridge_CanSavePage(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& j_url) {
  GURL url(ConvertJavaStringToUTF8(env, j_url));
  return OfflinePageModel::CanSaveURL(url);
}

static ScopedJavaLocalRef<jobject>
JNI_OfflinePageBridge_GetOfflinePageBridgeForProfile(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);

  // Return null if there is no reasonable context for the provided Java
  // profile.
  if (profile == nullptr)
    return ScopedJavaLocalRef<jobject>();

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(profile);

  // Return null if we cannot get an offline page model for provided profile.
  if (offline_page_model == nullptr)
    return ScopedJavaLocalRef<jobject>();

  OfflinePageBridge* bridge = static_cast<OfflinePageBridge*>(
      offline_page_model->GetUserData(kOfflinePageBridgeKey));
  if (!bridge) {
    bridge = new OfflinePageBridge(env, profile, offline_page_model);
    offline_page_model->SetUserData(kOfflinePageBridgeKey,
                                    base::WrapUnique(bridge));
  }

  return ScopedJavaLocalRef<jobject>(bridge->java_ref());
}

// static
ScopedJavaLocalRef<jobject> OfflinePageBridge::ConvertToJavaOfflinePage(
    JNIEnv* env,
    const OfflinePageItem& offline_page) {
  return JNI_SavePageRequest_ToJavaOfflinePageItem(env, offline_page);
}

// static
std::string OfflinePageBridge::GetEncodedOriginApp(
    const content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab)
    return "";
  JNIEnv* env = base::android::AttachCurrentThread();
  return ConvertJavaStringToUTF8(
      env,
      Java_OfflinePageBridge_getEncodedOriginApp(env, tab->GetJavaObject()));
}

OfflinePageBridge::OfflinePageBridge(JNIEnv* env,
                                     content::BrowserContext* browser_context,
                                     OfflinePageModel* offline_page_model)
    : browser_context_(browser_context),
      offline_page_model_(offline_page_model),
      weak_ptr_factory_(this) {
  ScopedJavaLocalRef<jobject> j_offline_page_bridge =
      Java_OfflinePageBridge_create(env, reinterpret_cast<jlong>(this));
  java_ref_.Reset(j_offline_page_bridge);

  NotifyIfDoneLoading();
  offline_page_model_->AddObserver(this);
}

OfflinePageBridge::~OfflinePageBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Native shutdown causes the destruction of |this|.
  Java_OfflinePageBridge_offlinePageBridgeDestroyed(env, java_ref_);
}

void OfflinePageBridge::OfflinePageModelLoaded(OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
  NotifyIfDoneLoading();
}

void OfflinePageBridge::OfflinePageAdded(OfflinePageModel* model,
                                         const OfflinePageItem& added_page) {
  DCHECK_EQ(offline_page_model_, model);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_OfflinePageBridge_offlinePageAdded(
      env, java_ref_,
      JNI_SavePageRequest_ToJavaOfflinePageItem(env, added_page));
}

void OfflinePageBridge::OfflinePageDeleted(
    const OfflinePageModel::DeletedPageInfo& page_info) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OfflinePageBridge_offlinePageDeleted(
      env, java_ref_,
      JNI_SavePageRequest_ToJavaDeletedPageInfo(env, page_info));
}

void OfflinePageBridge::GetAllPages(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  DCHECK(j_result_obj);
  DCHECK(j_callback_obj);

  ScopedJavaGlobalRef<jobject> j_result_ref;
  j_result_ref.Reset(env, j_result_obj);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  offline_page_model_->GetAllPages(base::BindOnce(
      &MultipleOfflinePageItemCallback, j_result_ref, j_callback_ref));
}

void OfflinePageBridge::GetPageByOfflineId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong offline_id,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);
  offline_page_model_->GetPageByOfflineId(
      offline_id,
      base::BindOnce(&SingleOfflinePageItemCallback, j_callback_ref));
}

std::vector<ClientId> getClientIdsFromObjectArrays(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& j_namespaces_array,
    const JavaParamRef<jobjectArray>& j_ids_array) {
  std::vector<std::string> name_spaces;
  std::vector<std::string> ids;
  base::android::AppendJavaStringArrayToStringVector(env, j_namespaces_array,
                                                     &name_spaces);
  base::android::AppendJavaStringArrayToStringVector(env, j_ids_array, &ids);
  DCHECK_EQ(name_spaces.size(), ids.size());
  std::vector<ClientId> client_ids;

  for (size_t i = 0; i < name_spaces.size(); i++) {
    offline_pages::ClientId client_id;
    client_id.name_space = name_spaces[i];
    client_id.id = ids[i];
    client_ids.emplace_back(client_id);
  }

  return client_ids;
}

void OfflinePageBridge::DeletePagesByClientId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobjectArray>& j_namespaces_array,
    const JavaParamRef<jobjectArray>& j_ids_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);
  std::vector<ClientId> client_ids =
      getClientIdsFromObjectArrays(env, j_namespaces_array, j_ids_array);
  offline_page_model_->DeletePagesByClientIds(
      client_ids, base::Bind(&DeletePageCallback, j_callback_ref));
}

void OfflinePageBridge::DeletePagesByClientIdAndOrigin(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobjectArray>& j_namespaces_array,
    const base::android::JavaParamRef<jobjectArray>& j_ids_array,
    const base::android::JavaParamRef<jstring>& j_origin,
    const base::android::JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);
  std::vector<ClientId> client_ids =
      getClientIdsFromObjectArrays(env, j_namespaces_array, j_ids_array);
  offline_page_model_->DeletePagesByClientIdsAndOrigin(
      client_ids, ConvertJavaStringToUTF8(j_origin),
      base::Bind(&DeletePageCallback, j_callback_ref));
}

void OfflinePageBridge::DeletePagesByOfflineId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jlongArray>& j_offline_ids_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);
  std::vector<int64_t> offline_ids;
  base::android::JavaLongArrayToInt64Vector(env, j_offline_ids_array,
                                            &offline_ids);
  offline_page_model_->DeletePagesByOfflineId(
      offline_ids, base::Bind(&DeletePageCallback, j_callback_ref));
}

void OfflinePageBridge::GetPagesByClientId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobjectArray>& j_namespaces_array,
    const JavaParamRef<jobjectArray>& j_ids_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_result_ref;
  j_result_ref.Reset(env, j_result_obj);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);
  std::vector<ClientId> client_ids =
      getClientIdsFromObjectArrays(env, j_namespaces_array, j_ids_array);
  offline_page_model_->GetPagesByClientIds(
      client_ids, base::BindOnce(&MultipleOfflinePageItemCallback, j_result_ref,
                                 j_callback_ref));
}

void OfflinePageBridge::GetPagesByRequestOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jstring>& j_request_origin,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_result_ref;
  j_result_ref.Reset(env, j_result_obj);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  std::string request_origin = ConvertJavaStringToUTF8(env, j_request_origin);

  offline_page_model_->GetPagesByRequestOrigin(
      request_origin, base::BindOnce(&MultipleOfflinePageItemCallback,
                                     j_result_ref, j_callback_ref));
}

void OfflinePageBridge::GetPagesByNamespace(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_result_ref(env, j_result_obj);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  std::string name_space = ConvertJavaStringToUTF8(env, j_namespace);

  offline_page_model_->GetPagesByNamespace(
      name_space, base::BindOnce(&MultipleOfflinePageItemCallback, j_result_ref,
                                 j_callback_ref));
}

void OfflinePageBridge::SelectPageForOnlineUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_online_url,
    int tab_id,
    const JavaParamRef<jobject>& j_callback_obj) {
  DCHECK(j_callback_obj);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  OfflinePageUtils::SelectPagesForURL(
      browser_context_, GURL(ConvertJavaStringToUTF8(env, j_online_url)),
      tab_id, base::BindOnce(&SelectPageCallback, j_callback_ref));
}

void OfflinePageBridge::SavePage(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 const JavaParamRef<jobject>& j_callback_obj,
                                 const JavaParamRef<jobject>& j_web_contents,
                                 const JavaParamRef<jstring>& j_namespace,
                                 const JavaParamRef<jstring>& j_client_id,
                                 const JavaParamRef<jstring>& j_origin) {
  DCHECK(j_callback_obj);
  DCHECK(j_web_contents);

  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  OfflinePageModel::SavePageParams save_page_params;
  std::unique_ptr<OfflinePageArchiver> archiver;

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (web_contents) {
    save_page_params.url = web_contents->GetLastCommittedURL();
    archiver.reset(new OfflinePageMHTMLArchiver());
  }

  save_page_params.client_id.name_space =
      ConvertJavaStringToUTF8(env, j_namespace);
  save_page_params.client_id.id = ConvertJavaStringToUTF8(env, j_client_id);
  save_page_params.is_background = false;
  save_page_params.request_origin = ConvertJavaStringToUTF8(env, j_origin);

  offline_page_model_->SavePage(
      save_page_params, std::move(archiver), web_contents,
      base::Bind(&SavePageCallback, j_callback_ref, save_page_params.url));
}

void OfflinePageBridge::SavePageLater(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_callback_obj,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_client_id,
    const JavaParamRef<jstring>& j_origin,
    jboolean user_requested) {
  DCHECK(j_callback_obj);
  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  offline_pages::ClientId client_id;
  client_id.name_space = ConvertJavaStringToUTF8(env, j_namespace);
  client_id.id = ConvertJavaStringToUTF8(env, j_client_id);

  RequestCoordinator* coordinator =
      offline_pages::RequestCoordinatorFactory::GetInstance()->
          GetForBrowserContext(browser_context_);

  RequestCoordinator::SavePageLaterParams params;
  params.url = GURL(ConvertJavaStringToUTF8(env, j_url));
  params.client_id = client_id;
  params.user_requested = static_cast<bool>(user_requested);
  params.availability =
      RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER;
  params.request_origin = ConvertJavaStringToUTF8(env, j_origin);

  coordinator->SavePageLater(
      params, base::BindOnce(&SavePageLaterCallback, j_callback_ref));
}

void OfflinePageBridge::PublishInternalPageByOfflineId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jlong j_offline_id,
    const base::android::JavaParamRef<jobject>& j_published_callback) {
  ScopedJavaGlobalRef<jobject> j_published_callback_ref;
  j_published_callback_ref.Reset(env, j_published_callback);

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context_);
  DCHECK(offline_page_model);

  offline_page_model->GetPageByOfflineId(
      j_offline_id,
      base::Bind(&OfflinePageBridge::PublishInternalArchive,
                 weak_ptr_factory_.GetWeakPtr(), j_published_callback_ref,
                 PublishSource::kPublishByOfflineId));
}

void OfflinePageBridge::PublishInternalPageByGuid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_guid,
    const base::android::JavaParamRef<jobject>& j_published_callback) {
  ScopedJavaGlobalRef<jobject> j_published_callback_ref;
  j_published_callback_ref.Reset(env, j_published_callback);

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context_);
  DCHECK(offline_page_model);

  offline_page_model->GetPageByGuid(
      ConvertJavaStringToUTF8(env, j_guid),
      base::BindOnce(&OfflinePageBridge::PublishInternalArchive,
                     weak_ptr_factory_.GetWeakPtr(), j_published_callback_ref,
                     PublishSource::kPublishByGuid));
}

void OfflinePageBridge::PublishInternalArchive(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const PublishSource publish_source,
    const OfflinePageItem* offline_page) {
  if (!offline_page) {
    PublishPageDone(j_callback_obj, base::FilePath(),
                    SavePageResult::CANCELLED);
    base::UmaHistogramEnumeration("OfflinePages.PublishArchive.PublishSource",
                                  publish_source);
    return;
  }

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context_);
  DCHECK(offline_page_model);

  // If it has already been published, bail out.
  if (!offline_page_model->IsArchiveInInternalDir(offline_page->file_path)) {
    PublishPageDone(j_callback_obj, offline_page->file_path,
                    SavePageResult::ALREADY_EXISTS);
    return;
  }

  std::unique_ptr<OfflinePageArchiver> archiver(new OfflinePageMHTMLArchiver());
  offline_page_model->PublishInternalArchive(
      *offline_page, std::move(archiver),
      base::BindOnce(&PublishPageDone, std::move(j_callback_obj)));
}

ScopedJavaLocalRef<jstring> OfflinePageBridge::GetOfflinePageHeaderForReload(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return ScopedJavaLocalRef<jstring>();

  const offline_pages::OfflinePageHeader* offline_header =
      offline_pages::OfflinePageUtils::GetOfflineHeaderFromWebContents(
          web_contents);
  if (!offline_header)
    return ScopedJavaLocalRef<jstring>();

  // Only replaces the reason field with "reload" value that is used to trigger
  // the network conditon check again in deciding whether to load the offline
  // page. All other fields in the offline header should still carry to the
  // reload request in order to keep the consistent behavior if we do decide to
  // load the offline page. For example, "id" field should be kept in order to
  // load the same offline version again if desired.
  offline_pages::OfflinePageHeader offline_header_for_reload = *offline_header;
  offline_header_for_reload.reason =
      offline_pages::OfflinePageHeader::Reason::RELOAD;
  return ScopedJavaLocalRef<jstring>(ConvertUTF8ToJavaString(
      env, offline_header_for_reload.GetCompleteHeaderString()));
}

jboolean OfflinePageBridge::IsShowingOfflinePreview(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return false;
  return offline_pages::OfflinePageUtils::IsShowingOfflinePreview(web_contents);
}

jboolean OfflinePageBridge::IsShowingDownloadButtonInErrorPage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return false;
  return offline_pages::OfflinePageUtils::IsShowingDownloadButtonInErrorPage(
      web_contents);
}

void OfflinePageBridge::GetRequestsInQueue(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  RequestCoordinator* coordinator =
      offline_pages::RequestCoordinatorFactory::GetInstance()
          ->GetForBrowserContext(browser_context_);

  if (!coordinator) {
    // Callback with null to signal that results are unavailable.
    const JavaParamRef<jobject> empty_result(nullptr);
    base::android::RunObjectCallbackAndroid(j_callback_obj, empty_result);
    return;
  }

  coordinator->GetAllRequests(
      base::BindOnce(&OnGetAllRequestsDone, j_callback_ref));
}

void OfflinePageBridge::RemoveRequestsFromQueue(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jlongArray>& j_request_ids_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  std::vector<int64_t> request_ids;
  base::android::JavaLongArrayToInt64Vector(env, j_request_ids_array,
                                            &request_ids);
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  RequestCoordinator* coordinator =
      offline_pages::RequestCoordinatorFactory::GetInstance()
          ->GetForBrowserContext(browser_context_);

  if (!coordinator) {
    // Callback with null to signal that results are unavailable.
    const JavaParamRef<jobject> empty_result(nullptr);
    base::android::RunObjectCallbackAndroid(j_callback_obj, empty_result);
    return;
  }

  coordinator->RemoveRequests(
      request_ids, base::BindOnce(&OnRemoveRequestsDone, j_callback_ref));
}

void OfflinePageBridge::WillCloseTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents) {
  DCHECK(j_web_contents);
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  DCHECK(web_contents);
  if (!web_contents)
    return;

  RecentTabHelper* tab_helper = RecentTabHelper::FromWebContents(web_contents);
  if (tab_helper)
    tab_helper->WillCloseTab();
}

void OfflinePageBridge::ScheduleDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_url,
    int ui_action,
    const JavaParamRef<jstring>& j_origin) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  OfflinePageUtils::ScheduleDownload(
      web_contents, ConvertJavaStringToUTF8(env, j_namespace),
      GURL(ConvertJavaStringToUTF8(env, j_url)),
      static_cast<OfflinePageUtils::DownloadUIActionFlags>(ui_action),
      ConvertJavaStringToUTF8(env, j_origin));
}

jboolean OfflinePageBridge::IsOfflinePage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  return offline_pages::OfflinePageUtils::GetOfflinePageFromWebContents(
             web_contents) != nullptr;
}

jboolean OfflinePageBridge::IsInPrivateDirectory(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_file_path) {
  base::FilePath file_path(ConvertJavaStringToUTF8(env, j_file_path));
  return offline_page_model_->IsArchiveInInternalDir(file_path);
}

jboolean OfflinePageBridge::IsUserRequestedDownloadNamespace(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_name_space) {
  std::string name_space(ConvertJavaStringToUTF8(env, j_name_space));
  return (offline_page_model_->GetPolicyController()->IsUserRequestedDownload(
      name_space));
}

ScopedJavaLocalRef<jobject> OfflinePageBridge::GetOfflinePage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  const offline_pages::OfflinePageItem* offline_page =
      offline_pages::OfflinePageUtils::GetOfflinePageFromWebContents(
          content::WebContents::FromJavaWebContents(j_web_contents));
  if (!offline_page)
    return ScopedJavaLocalRef<jobject>();

  return offline_pages::android::OfflinePageBridge::ConvertToJavaOfflinePage(
      env, *offline_page);
}

void OfflinePageBridge::CheckForNewOfflineContent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const jlong j_timestamp_millis,
    const JavaParamRef<jobject>& j_callback_obj) {
  base::Time pages_created_after = base::Time::FromJavaTime(j_timestamp_millis);
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  offline_page_model_->GetPagesSupportedByDownloads(base::Bind(
      &CheckForNewOfflineContentCallback, pages_created_after, j_callback_ref));
}

void OfflinePageBridge::GetLoadUrlParamsByOfflineId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong j_offline_id,
    jint launch_location,
    const base::android::JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  offline_page_model_->GetPageByOfflineId(
      j_offline_id,
      base::Bind(&OfflinePageBridge::GetPageByOfflineIdDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 static_cast<offline_items_collection::LaunchLocation>(
                     launch_location),
                 j_callback_ref));
}

void OfflinePageBridge::GetLoadUrlParamsForOpeningMhtmlFileOrContent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_url,
    const base::android::JavaParamRef<jobject>& j_callback_obj) {
  GURL url(ConvertJavaStringToUTF8(env, j_url));
  base::FilePath file_path;
  if (url.SchemeIsFile()) {
    net::FileURLToFilePath(url, &file_path);
  } else {
    DCHECK(url.SchemeIs("content"));
    // Content URI can be embeded in the file path and FileStream knows how to
    // read it.
    file_path = base::FilePath(url.spec());
  }

  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&ArchiveValidator::GetSizeAndComputeDigest, file_path),
      base::Bind(&OfflinePageBridge::GetSizeAndComputeDigestDone,
                 weak_ptr_factory_.GetWeakPtr(), j_callback_ref, url));
}

jboolean OfflinePageBridge::IsShowingTrustedOfflinePage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return false;
  return offline_pages::OfflinePageUtils::IsShowingTrustedOfflinePage(
      web_contents);
}

void OfflinePageBridge::GetPageByOfflineIdDone(
    offline_items_collection::LaunchLocation launch_location,
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageItem* offline_page) {
  if (!offline_page) {
    base::android::RunObjectCallbackAndroid(j_callback_obj, nullptr);
    return;
  }

  if (offline_page_model_->IsArchiveInInternalDir(offline_page->file_path)) {
    ValidateFileCallback(launch_location, j_callback_obj,
                         offline_page->offline_id, offline_page->url,
                         offline_page->file_path, true /* is_trusted*/);
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&ArchiveValidator::ValidateFile, offline_page->file_path,
                 offline_page->file_size, offline_page->digest),
      base::Bind(&ValidateFileCallback, launch_location, j_callback_obj,
                 offline_page->offline_id, offline_page->url,
                 offline_page->file_path));
}

void OfflinePageBridge::GetSizeAndComputeDigestDone(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const GURL& intent_url,
    std::pair<int64_t, std::string> size_and_digest) {
  // If size or digest can't be obtained, launch the intent URL.
  if (!size_and_digest.first || size_and_digest.second.empty()) {
    RunLoadUrlParamsCallbackAndroid(j_callback_obj, intent_url,
                                    offline_pages::OfflinePageHeader());
    return;
  }

  offline_page_model_->GetPageBySizeAndDigest(
      size_and_digest.first, size_and_digest.second,
      base::BindOnce(&OfflinePageBridge::GetPageBySizeAndDigestDone,
                     weak_ptr_factory_.GetWeakPtr(), j_callback_obj,
                     intent_url));
}

void OfflinePageBridge::GetPageBySizeAndDigestDone(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const GURL& intent_url,
    const OfflinePageItem* offline_page) {
  GURL launch_url;
  offline_pages::OfflinePageHeader offline_header;
  if (offline_page) {
    launch_url = offline_page->url;
    offline_header.reason =
        intent_url.SchemeIsFile()
            ? offline_pages::OfflinePageHeader::Reason::FILE_URL_INTENT
            : offline_pages::OfflinePageHeader::Reason::CONTENT_URL_INTENT;
    offline_header.need_to_persist = true;
    offline_header.id = base::Int64ToString(offline_page->offline_id);
    offline_header.intent_url = intent_url;
  } else {
    // If the offline page can't be found, launch the intent URL.
    launch_url = intent_url;
  }
  RunLoadUrlParamsCallbackAndroid(j_callback_obj, launch_url, offline_header);
}

void OfflinePageBridge::AcquireFileAccessPermission(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents) {
    base::android::RunBooleanCallbackAndroid(j_callback_ref, false);
    return;
  }
  OfflinePageUtils::AcquireFileAccessPermission(
      web_contents, base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                                   j_callback_ref));
}

void OfflinePageBridge::NotifyIfDoneLoading() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OfflinePageBridge_offlinePageModelLoaded(env, java_ref_);
}


ScopedJavaLocalRef<jobject> OfflinePageBridge::CreateClientId(
    JNIEnv* env,
    const ClientId& client_id) const {
  return Java_OfflinePageBridge_createClientId(
      env, ConvertUTF8ToJavaString(env, client_id.name_space),
      ConvertUTF8ToJavaString(env, client_id.id));
}

}  // namespace android
}  // namespace offline_pages

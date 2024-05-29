// Copyright 2015 The Chromium Authors
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
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/offline_pages/recent_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_key_android.h"
#include "components/offline_pages/core/archive_validator.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/page_criteria.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/OfflinePageBridge_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace offline_pages {
namespace android {

namespace {

const char kOfflinePageBridgeKey[] = "offline-page-bridge";

ScopedJavaLocalRef<jobject> JNI_SavePageRequest_ToJavaOfflinePageItem(
    JNIEnv* env,
    const OfflinePageItem& offline_page) {
  return Java_OfflinePageBridge_createOfflinePageItem(
      env, offline_page.url.spec(), offline_page.offline_id,
      offline_page.client_id.name_space, offline_page.client_id.id,
      offline_page.title, offline_page.file_path.value(),
      offline_page.file_size,
      offline_page.creation_time.InMillisecondsSinceUnixEpoch(),
      offline_page.access_count,
      offline_page.last_access_time.InMillisecondsSinceUnixEpoch(),
      offline_page.request_origin);
}

ScopedJavaLocalRef<jobject> JNI_SavePageRequest_ToJavaDeletedPageInfo(
    JNIEnv* env,
    const OfflinePageItem& deleted_page) {
  return Java_OfflinePageBridge_createDeletedPageInfo(
      env, deleted_page.offline_id, deleted_page.client_id.name_space,
      deleted_page.client_id.id, deleted_page.request_origin);
}

void MultipleOfflinePageItemCallback(
    const ScopedJavaGlobalRef<jobject>& j_result_obj,
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageModel::MultipleOfflinePageItemResult& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  OfflinePageBridge::AddOfflinePageItemsToJavaList(env, j_result_obj, result);
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_result_obj);
}

void SavePageCallback(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                      const GURL& url,
                      OfflinePageModel::SavePageResult result,
                      int64_t offline_id) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_SavePageCallback_onSavePageDone(
      env, j_callback_obj, static_cast<int>(result), url.spec(), offline_id);
}

void DeletePageCallback(const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                        OfflinePageModel::DeletePageResult result) {
  base::android::RunIntCallbackAndroid(j_callback_obj,
                                       static_cast<int32_t>(result));
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

void RunLoadUrlParamsCallbackAndroid(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const GURL& url,
    const OfflinePageHeader& offline_page_header) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> loadUrlParams =
      Java_OfflinePageBridge_createLoadUrlParams(
          env, url.spec(), offline_page_header.GetHeaderKeyString(),
          offline_page_header.GetHeaderValueString());
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
  // page. If the file path is content URI, directly open it. Otherwise, the
  // launch url will be the file URL pointing to the archive file of the offline
  // page.
  GURL launch_url;
  if (is_trusted)
    launch_url = url;
  else if (file_path.IsContentUri())
    launch_url = GURL(file_path.value());
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
      NOTREACHED_IN_MIGRATION();
      break;
    case offline_items_collection::LaunchLocation::DOWNLOAD_INTERSTITIAL:
      offline_header.reason =
          offline_pages::OfflinePageHeader::Reason::DOWNLOAD;
      break;
  }
  offline_header.need_to_persist = true;
  offline_header.id = base::NumberToString(offline_id);

  RunLoadUrlParamsCallbackAndroid(j_callback_obj, launch_url, offline_header);
}

void PublishPageDone(
    const ScopedJavaGlobalRef<jobject>& j_published_callback_obj,
    const base::FilePath& file_path,
    SavePageResult result) {
  base::FilePath file_path_or_empty;
  if (result != SavePageResult::SUCCESS)
    file_path_or_empty = file_path;

  base::android::RunStringCallbackAndroid(j_published_callback_obj,
                                          file_path.value());
}

}  // namespace

static jboolean JNI_OfflinePageBridge_CanSavePage(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_url) {
  return OfflinePageModel::CanSaveURL(
      url::GURLAndroid::ToNativeGURL(env, j_url));
}

static ScopedJavaLocalRef<jobject>
JNI_OfflinePageBridge_GetOfflinePageBridgeForProfileKey(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile_key) {
  ProfileKey* profile_key =
      ProfileKeyAndroid::FromProfileKeyAndroid(j_profile_key);

  // Return null if there is no reasonable context for the provided Java
  // profile.
  if (profile_key == nullptr)
    return ScopedJavaLocalRef<jobject>();

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForKey(profile_key);

  // Return null if we cannot get an offline page model for provided profile.
  if (offline_page_model == nullptr)
    return ScopedJavaLocalRef<jobject>();

  OfflinePageBridge* bridge = static_cast<OfflinePageBridge*>(
      offline_page_model->GetUserData(kOfflinePageBridgeKey));
  if (!bridge) {
    bridge = new OfflinePageBridge(env, profile_key, offline_page_model);
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
void OfflinePageBridge::AddOfflinePageItemsToJavaList(
    JNIEnv* env,
    const JavaRef<jobject>& j_result_obj,
    const std::vector<OfflinePageItem>& offline_pages) {
  for (const OfflinePageItem& offline_page : offline_pages) {
    Java_OfflinePageBridge_createOfflinePageAndAddToList(
        env, j_result_obj, offline_page.url.spec(), offline_page.offline_id,
        offline_page.client_id.name_space, offline_page.client_id.id,
        offline_page.title, offline_page.file_path.value(),
        offline_page.file_size,
        offline_page.creation_time.InMillisecondsSinceUnixEpoch(),
        offline_page.access_count,
        offline_page.last_access_time.InMillisecondsSinceUnixEpoch(),
        offline_page.request_origin);
  }
}

// static
std::string OfflinePageBridge::GetEncodedOriginApp(
    const content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab)
    return "";
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_OfflinePageBridge_getEncodedOriginApp(env, tab->GetJavaObject());
}

OfflinePageBridge::OfflinePageBridge(JNIEnv* env,
                                     SimpleFactoryKey* key,
                                     OfflinePageModel* offline_page_model)
    : key_(key), offline_page_model_(offline_page_model) {
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

void OfflinePageBridge::OfflinePageDeleted(const OfflinePageItem& item) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OfflinePageBridge_offlinePageDeleted(
      env, java_ref_, JNI_SavePageRequest_ToJavaDeletedPageInfo(env, item));
}

void OfflinePageBridge::GetAllPages(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  DCHECK(j_result_obj);
  DCHECK(j_callback_obj);

  offline_page_model_->GetAllPages(
      base::BindOnce(&MultipleOfflinePageItemCallback,
                     ScopedJavaGlobalRef<jobject>(j_result_obj),
                     ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void OfflinePageBridge::GetPageByOfflineId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong offline_id,
    const JavaParamRef<jobject>& j_callback_obj) {
  offline_page_model_->GetPageByOfflineId(
      offline_id, base::BindOnce(&SingleOfflinePageItemCallback,
                                 ScopedJavaGlobalRef<jobject>(j_callback_obj)));
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
  PageCriteria criteria;
  criteria.client_ids =
      getClientIdsFromObjectArrays(env, j_namespaces_array, j_ids_array);
  offline_page_model_->DeletePagesWithCriteria(
      criteria, base::BindOnce(&DeletePageCallback,
                               ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void OfflinePageBridge::DeletePagesByClientIdAndOrigin(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobjectArray>& j_namespaces_array,
    const base::android::JavaParamRef<jobjectArray>& j_ids_array,
    std::string& origin,
    const base::android::JavaParamRef<jobject>& j_callback_obj) {
  PageCriteria criteria;
  criteria.client_ids =
      getClientIdsFromObjectArrays(env, j_namespaces_array, j_ids_array);
  criteria.request_origin = origin;
  offline_page_model_->DeletePagesWithCriteria(
      criteria, base::BindOnce(&DeletePageCallback,
                               ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void OfflinePageBridge::DeletePagesByOfflineId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jlongArray>& j_offline_ids_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  std::vector<int64_t> offline_ids;
  base::android::JavaLongArrayToInt64Vector(env, j_offline_ids_array,
                                            &offline_ids);

  PageCriteria criteria;
  criteria.offline_ids = std::move(offline_ids);
  offline_page_model_->DeletePagesWithCriteria(
      criteria, base::BindOnce(&DeletePageCallback,
                               ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void OfflinePageBridge::GetPagesByClientId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobjectArray>& j_namespaces_array,
    const JavaParamRef<jobjectArray>& j_ids_array,
    const JavaParamRef<jobject>& j_callback_obj) {
  std::vector<ClientId> client_ids =
      getClientIdsFromObjectArrays(env, j_namespaces_array, j_ids_array);
  PageCriteria criteria;
  criteria.client_ids = client_ids;
  offline_page_model_->GetPagesWithCriteria(
      criteria, base::BindOnce(&MultipleOfflinePageItemCallback,
                               ScopedJavaGlobalRef<jobject>(j_result_obj),
                               ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void OfflinePageBridge::GetPagesByRequestOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    std::string& request_origin,
    const JavaParamRef<jobject>& j_callback_obj) {
  PageCriteria criteria;
  criteria.request_origin = request_origin;
  offline_page_model_->GetPagesWithCriteria(
      criteria, base::BindOnce(&MultipleOfflinePageItemCallback,
                               ScopedJavaGlobalRef<jobject>(j_result_obj),
                               ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void OfflinePageBridge::GetPagesByNamespace(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    std::string& namespace_str,
    const JavaParamRef<jobject>& j_callback_obj) {
  PageCriteria criteria;
  criteria.client_namespaces = std::vector<std::string>{namespace_str};
  offline_page_model_->GetPagesWithCriteria(
      criteria, base::BindOnce(&MultipleOfflinePageItemCallback,
                               ScopedJavaGlobalRef<jobject>(j_result_obj),
                               ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void OfflinePageBridge::SelectPageForOnlineUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_online_url,
    int tab_id,
    const JavaParamRef<jobject>& j_callback_obj) {
  DCHECK(j_callback_obj);

  OfflinePageUtils::SelectPagesForURL(
      key_, url::GURLAndroid::ToNativeGURL(env, j_online_url), tab_id,
      base::BindOnce(&SelectPageCallback,
                     ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void OfflinePageBridge::SavePage(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 const JavaParamRef<jobject>& j_callback_obj,
                                 const JavaParamRef<jobject>& j_web_contents,
                                 std::string& namespace_str,
                                 std::string& client_id,
                                 std::string& origin) {
  DCHECK(j_callback_obj);
  DCHECK(j_web_contents);

  OfflinePageModel::SavePageParams save_page_params;
  std::unique_ptr<OfflinePageArchiver> archiver;

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (web_contents) {
    save_page_params.url = web_contents->GetLastCommittedURL();
    archiver = std::make_unique<OfflinePageMHTMLArchiver>();
  }

  save_page_params.client_id.name_space = namespace_str;
  save_page_params.client_id.id = client_id;
  save_page_params.is_background = false;
  save_page_params.request_origin = origin;

  offline_page_model_->SavePage(
      save_page_params, std::move(archiver), web_contents,
      base::BindOnce(&SavePageCallback,
                     ScopedJavaGlobalRef<jobject>(j_callback_obj),
                     save_page_params.url));
}

void OfflinePageBridge::PublishInternalPageByOfflineId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jlong j_offline_id,
    const base::android::JavaParamRef<jobject>& j_published_callback) {
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForKey(key_);
  DCHECK(offline_page_model);

  offline_page_model->GetPageByOfflineId(
      j_offline_id,
      base::BindOnce(&OfflinePageBridge::PublishInternalArchive,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_published_callback)));
}

void OfflinePageBridge::PublishInternalPageByGuid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    std::string& guid,
    const base::android::JavaParamRef<jobject>& j_published_callback) {
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForKey(key_);
  DCHECK(offline_page_model);
  PageCriteria criteria;
  criteria.guid = guid;
  criteria.maximum_matches = 1;
  offline_page_model->GetPagesWithCriteria(
      criteria,
      base::BindOnce(&OfflinePageBridge::PublishInternalArchiveOfFirstItem,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_published_callback)));
}

void OfflinePageBridge::PublishInternalArchiveOfFirstItem(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const std::vector<OfflinePageItem>& offline_pages) {
  // Should only ever be called with 0 or 1 page.
  DCHECK_GE(1UL, offline_pages.size());
  if (offline_pages.empty()) {
    PublishInternalArchive(j_callback_obj, nullptr);
    return;
  }
  PublishInternalArchive(j_callback_obj, &offline_pages[0]);
}

void OfflinePageBridge::PublishInternalArchive(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const OfflinePageItem* offline_page) {
  if (!offline_page) {
    PublishPageDone(j_callback_obj, base::FilePath(),
                    SavePageResult::CANCELLED);
    return;
  }
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForKey(key_);
  DCHECK(offline_page_model);

  // If it has already been published, bail out.
  if (!offline_page_model->IsArchiveInInternalDir(offline_page->file_path)) {
    PublishPageDone(j_callback_obj, offline_page->file_path,
                    SavePageResult::ALREADY_EXISTS);
    return;
  }

  offline_page_model->PublishInternalArchive(
      *offline_page,
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
    std::string& namespace_str,
    std::string& url_spec,
    int ui_action,
    std::string& origin) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  OfflinePageUtils::ScheduleDownload(
      web_contents, namespace_str, GURL(url_spec),
      static_cast<OfflinePageUtils::DownloadUIActionFlags>(ui_action), origin);
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
    std::string& file_path) {
  return offline_page_model_->IsArchiveInInternalDir(base::FilePath(file_path));
}

jboolean OfflinePageBridge::IsTemporaryNamespace(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    std::string& name_space) {
  return GetPolicy(name_space).lifetime_type == LifetimeType::TEMPORARY;
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

void OfflinePageBridge::GetLoadUrlParamsByOfflineId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong j_offline_id,
    jint launch_location,
    const base::android::JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref(j_callback_obj);

  offline_page_model_->GetPageByOfflineId(
      j_offline_id,
      base::BindOnce(&OfflinePageBridge::GetPageByOfflineIdDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     static_cast<offline_items_collection::LaunchLocation>(
                         launch_location),
                     j_callback_ref));
}

void OfflinePageBridge::GetLoadUrlParamsForOpeningMhtmlFileOrContent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    std::string& url_spec,
    const base::android::JavaParamRef<jobject>& j_callback_obj) {
  GURL url(url_spec);
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
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ArchiveValidator::GetSizeAndComputeDigest, file_path),
      base::BindOnce(&OfflinePageBridge::GetSizeAndComputeDigestDone,
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

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ArchiveValidator::ValidateFile, offline_page->file_path,
                     offline_page->file_size, offline_page->digest),
      base::BindOnce(&ValidateFileCallback, launch_location, j_callback_obj,
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
  PageCriteria criteria;
  criteria.file_size = size_and_digest.first;
  criteria.digest = size_and_digest.second;
  criteria.maximum_matches = 1;
  offline_page_model_->GetPagesWithCriteria(
      criteria, base::BindOnce(&OfflinePageBridge::GetPageBySizeAndDigestDone,
                               weak_ptr_factory_.GetWeakPtr(), j_callback_obj,
                               intent_url));
}

void OfflinePageBridge::GetPageBySizeAndDigestDone(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj,
    const GURL& intent_url,
    const std::vector<OfflinePageItem>& offline_pages) {
  GURL launch_url;
  offline_pages::OfflinePageHeader offline_header;
  if (!offline_pages.empty()) {
    const OfflinePageItem& offline_page = offline_pages[0];
    launch_url = offline_page.url;
    offline_header.reason =
        intent_url.SchemeIsFile()
            ? offline_pages::OfflinePageHeader::Reason::FILE_URL_INTENT
            : offline_pages::OfflinePageHeader::Reason::CONTENT_URL_INTENT;
    offline_header.need_to_persist = true;
    offline_header.id = base::NumberToString(offline_page.offline_id);
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
  return Java_OfflinePageBridge_createClientId(env, client_id.name_space,
                                               client_id.id);
}

}  // namespace android
}  // namespace offline_pages

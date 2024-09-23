// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_BRIDGE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_BRIDGE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/offline_items_collection/core/launch_location.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"

class SimpleFactoryKey;

namespace content {
class WebContents;
}

namespace offline_pages {
namespace android {

/**
 * Bridge between C++ and Java for exposing native implementation of offline
 * pages model in managed code.
 */
class OfflinePageBridge : public OfflinePageModel::Observer,
                          public base::SupportsUserData::Data {
 public:
  static base::android::ScopedJavaLocalRef<jobject> ConvertToJavaOfflinePage(
      JNIEnv* env,
      const OfflinePageItem& offline_page);

  static void AddOfflinePageItemsToJavaList(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_result_obj,
      const std::vector<OfflinePageItem>& offline_pages);

  static std::string GetEncodedOriginApp(
      const content::WebContents* web_contents);

  OfflinePageBridge(JNIEnv* env,
                    SimpleFactoryKey* key,
                    OfflinePageModel* offline_page_model);

  OfflinePageBridge(const OfflinePageBridge&) = delete;
  OfflinePageBridge& operator=(const OfflinePageBridge&) = delete;

  ~OfflinePageBridge() override;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageAdded(OfflinePageModel* model,
                        const OfflinePageItem& added_page) override;
  void OfflinePageDeleted(const OfflinePageItem& item) override;

  void GetAllPages(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& j_result_obj,
                   const base::android::JavaParamRef<jobject>& j_callback_obj);

  void GetPageByOfflineId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong offline_id,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void DeletePagesByClientId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobjectArray>& j_namespaces_array,
      const base::android::JavaParamRef<jobjectArray>& j_ids_array,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void DeletePagesByClientIdAndOrigin(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobjectArray>& j_namespaces_array,
      const base::android::JavaParamRef<jobjectArray>& j_ids_array,
      std::string& origin,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void DeletePagesByOfflineId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jlongArray>& j_offline_ids_array,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void GetPagesByClientId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_result_obj,
      const base::android::JavaParamRef<jobjectArray>& j_namespaces_array,
      const base::android::JavaParamRef<jobjectArray>& j_ids_array,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void GetPagesByRequestOrigin(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_result_obj,
      std::string& request_origin,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void GetPagesByNamespace(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_result_obj,
      std::string& namespace_str,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void SelectPageForOnlineUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_online_url,
      int tab_id,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void SavePage(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                const base::android::JavaParamRef<jobject>& j_callback_obj,
                const base::android::JavaParamRef<jobject>& j_web_contents,
                std::string& namespace_str,
                std::string& client_id,
                std::string& origin);

  void PublishInternalPageByOfflineId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const jlong j_offline_id,
      const base::android::JavaParamRef<jobject>& j_published_callback);

  void PublishInternalPageByGuid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      std::string& guid,
      const base::android::JavaParamRef<jobject>& j_published_callback);

  jboolean IsShowingOfflinePreview(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  jboolean IsShowingDownloadButtonInErrorPage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  base::android::ScopedJavaLocalRef<jstring> GetOfflinePageHeaderForReload(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  void WillCloseTab(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jobject>& j_web_contents);
  void ScheduleDownload(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      std::string& namespace_str,
      std::string& url_spec,
      int ui_action,
      std::string& origin);

  base::android::ScopedJavaGlobalRef<jobject> java_ref() { return java_ref_; }

  jboolean IsOfflinePage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  jboolean IsInPrivateDirectory(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                std::string& file_path);

  jboolean IsTemporaryNamespace(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                std::string& name_space);

  base::android::ScopedJavaLocalRef<jobject> GetOfflinePage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  void GetLoadUrlParamsByOfflineId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong j_offline_id,
      jint launch_location,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void GetLoadUrlParamsForOpeningMhtmlFileOrContent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      std::string& url_spec,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  jboolean IsShowingTrustedOfflinePage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  void AcquireFileAccessPermission(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

 private:
  void GetPageByOfflineIdDone(
      offline_items_collection::LaunchLocation launch_location,
      const base::android::ScopedJavaGlobalRef<jobject>& j_callback_obj,
      const OfflinePageItem* offline_page);

  void GetSizeAndComputeDigestDone(
      const base::android::ScopedJavaGlobalRef<jobject>& j_callback_obj,
      const GURL& intent_url,
      std::pair<int64_t, std::string> size_and_digest);

  void GetPageBySizeAndDigestDone(
      const base::android::ScopedJavaGlobalRef<jobject>& j_callback_obj,
      const GURL& intent_url,
      const std::vector<OfflinePageItem>& offline_pages);

  void NotifyIfDoneLoading() const;

  base::android::ScopedJavaLocalRef<jobject> CreateClientId(
      JNIEnv* env,
      const ClientId& clientId) const;

  void PublishInternalArchive(
      const base::android::ScopedJavaGlobalRef<jobject>& j_callback_obj,
      const OfflinePageItem* offline_pages);

  void PublishInternalArchiveOfFirstItem(
      const base::android::ScopedJavaGlobalRef<jobject>& j_callback_obj,
      const std::vector<OfflinePageItem>& offline_pages);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  // Not owned.
  raw_ptr<SimpleFactoryKey> key_;
  // Not owned.
  raw_ptr<OfflinePageModel> offline_page_model_;

  base::WeakPtrFactory<OfflinePageBridge> weak_ptr_factory_{this};
};

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_BRIDGE_H_

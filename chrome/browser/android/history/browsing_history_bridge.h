// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_BROWSING_HISTORY_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_HISTORY_BROWSING_HISTORY_BRIDGE_H_

#include <memory>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/history/profile_based_browsing_history_driver.h"

using base::android::JavaParamRef;

// The bridge for fetching browsing history information for the Android
// history UI. This queries the history::BrowsingHistoryService and listens
// for callbacks.
class BrowsingHistoryBridge : public ProfileBasedBrowsingHistoryDriver {
 public:
  explicit BrowsingHistoryBridge(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 Profile* profile);

  BrowsingHistoryBridge(const BrowsingHistoryBridge&) = delete;
  BrowsingHistoryBridge& operator=(const BrowsingHistoryBridge&) = delete;

  void Destroy(JNIEnv*, const JavaParamRef<jobject>&);

  void QueryHistory(JNIEnv* env,
                    const JavaParamRef<jobject>& obj,
                    const JavaParamRef<jobject>& j_result_obj,
                    jstring j_query,
                    const JavaParamRef<jstring>& j_app_id,
                    jboolean j_host_only);

  void QueryHistoryContinuation(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jobject>& j_result_obj);

  void GetAllAppIds(JNIEnv* env,
                    const JavaParamRef<jobject>& obj,
                    const JavaParamRef<jobject>& j_result_obj);

  void GetLastVisitToHostBeforeRecentNavigations(
      JNIEnv* env,
      const JavaParamRef<jobject>& obj,
      jstring j_host_name,
      const JavaParamRef<jobject>& jcallback_);

  // Adds a HistoryEntry with the |j_url| and |j_native_timestamps| to the list
  // of items being removed. The removal will not be committed until
  // ::removeItems() is called.
  void MarkItemForRemoval(JNIEnv* env,
                          const JavaParamRef<jobject>& obj,
                          const JavaParamRef<jobject>& j_url,
                          const JavaParamRef<jstring>& j_app_id,
                          const JavaParamRef<jlongArray>& j_native_timestamps);

  // Removes all items that have been marked for removal through
  // ::markItemForRemoval().
  void RemoveItems(JNIEnv* env,
                   const JavaParamRef<jobject>& obj);

  // BrowsingHistoryDriver implementation.
  void OnQueryComplete(
      const std::vector<history::BrowsingHistoryService::HistoryEntry>& results,
      const history::BrowsingHistoryService::QueryResultsInfo&
          query_results_info,
      base::OnceClosure continuation_closure) override;
  void OnRemoveVisitsComplete() override;
  void OnRemoveVisitsFailed() override;
  void HistoryDeleted() override;
  void HasOtherFormsOfBrowsingHistory(
      bool has_other_forms, bool has_synced_results) override;
  void OnGetAllAppIds(const std::vector<std::string>& app_ids) override;

  // ProfileBasedBrowsingHistoryDriver implementation.
  Profile* GetProfile() override;

 private:
  ~BrowsingHistoryBridge() override;

  std::unique_ptr<history::BrowsingHistoryService> browsing_history_service_;
  base::android::ScopedJavaGlobalRef<jobject> j_history_service_obj_;
  base::android::ScopedJavaGlobalRef<jobject> j_query_result_obj_;
  base::android::ScopedJavaGlobalRef<jobject> j_app_ids_result_obj_;

  std::vector<history::BrowsingHistoryService::HistoryEntry> items_to_remove_;

  raw_ptr<Profile> profile_;

  base::OnceClosure query_history_continuation_;
};

#endif  // CHROME_BROWSER_ANDROID_HISTORY_BROWSING_HISTORY_BRIDGE_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_BRIDGE_H_
#define CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_BRIDGE_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/query_clusters_state.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

namespace history_clusters {

// Native JNI bridge that provides access to HistoryClusters data. This bridge
// is instantiated lazily via GetForProfile and is owned by the associated
// HistoryClustersService via UserData.
class HistoryClustersBridge : public base::SupportsUserData::Data {
 public:
  HistoryClustersBridge(JNIEnv* env,
                        HistoryClustersService* history_clusters_service);
  // Start a new query for history clusters, fetching the first page of results
  // and calling back to j_callback when done.
  void QueryClusters(JNIEnv* env,
                     const JavaRef<jobject>& j_this,
                     const JavaRef<jstring>& j_query,
                     const JavaRef<jobject>& j_callback);
  // Continue the current query for history clusters, fetching the next page of
  // results and calling back to j_callback when done.
  void LoadMoreClusters(JNIEnv* env,
                        const JavaRef<jobject>& j_this,
                        const JavaRef<jstring>& j_query,
                        const JavaRef<jobject>& j_callback);
  // Destroy the bridge.
  void Destroy(JNIEnv* j_env);

  base::android::ScopedJavaGlobalRef<jobject> java_ref() { return java_ref_; }

  HistoryClustersBridge(const HistoryClustersBridge&) = delete;
  HistoryClustersBridge& operator=(const HistoryClustersBridge&) = delete;

  ~HistoryClustersBridge() override;

 private:
  void ClustersQueryDone(JNIEnv* env,
                         const JavaRef<jobject>& j_this,
                         const JavaRef<jobject>& j_callback,
                         const std::string& query,
                         std::vector<history::Cluster> clusters,
                         bool can_load_more,
                         bool is_continuation);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  raw_ptr<HistoryClustersService> history_clusters_service_;
  base::CancelableTaskTracker query_task_tracker_;
  std::unique_ptr<QueryClustersState> query_clusters_state_;
  base::WeakPtrFactory<HistoryClustersBridge> weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_BRIDGE_H_

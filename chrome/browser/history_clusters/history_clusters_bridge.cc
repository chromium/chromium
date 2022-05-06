// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_bridge.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/history_clusters/jni_headers/HistoryClustersBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/history/core/browser/history_types.h"
#include "url/android/gurl_android.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {
const char kHistoryClustersBridgeKey[] = "history-clusters-bridge";
}

namespace history_clusters {

static ScopedJavaLocalRef<jobject> JNI_HistoryClustersBridge_GetForProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);

  HistoryClustersService* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile);

  if (history_clusters_service == nullptr)
    return ScopedJavaLocalRef<jobject>();

  HistoryClustersBridge* bridge = static_cast<HistoryClustersBridge*>(
      history_clusters_service->GetUserData(kHistoryClustersBridgeKey));
  if (!bridge) {
    bridge = new HistoryClustersBridge(env, history_clusters_service);
    history_clusters_service->SetUserData(kHistoryClustersBridgeKey,
                                          base::WrapUnique(bridge));
  }

  return ScopedJavaLocalRef<jobject>(bridge->java_ref());
}

HistoryClustersBridge::HistoryClustersBridge(
    JNIEnv* env,
    HistoryClustersService* history_clusters_service)
    : history_clusters_service_(history_clusters_service) {
  ScopedJavaLocalRef<jobject> j_history_clusters_bridge =
      Java_HistoryClustersBridge_create(env, reinterpret_cast<jlong>(this));
  java_ref_.Reset(j_history_clusters_bridge);
}

HistoryClustersBridge::~HistoryClustersBridge() = default;

void HistoryClustersBridge::QueryClusters(JNIEnv* env,
                                          const JavaRef<jobject>& j_this,
                                          const JavaRef<jstring>& j_query,
                                          const JavaRef<jobject>& j_callback) {
  query_task_tracker_.TryCancelAll();
  query_clusters_state_ = std::make_unique<QueryClustersState>(
      history_clusters_service_->GetWeakPtr(),
      base::android::ConvertJavaStringToUTF8(env, j_query));
  LoadMoreClusters(env, j_this, j_query, j_callback);
}

void HistoryClustersBridge::LoadMoreClusters(
    JNIEnv* env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jstring>& j_query,
    const JavaRef<jobject>& j_callback) {
  const std::string& query =
      base::android::ConvertJavaStringToUTF8(env, j_query);
  if (query_clusters_state_) {
    DCHECK_EQ(query, query_clusters_state_->query());
    QueryClustersState::ResultCallback callback =
        base::BindOnce(&HistoryClustersBridge::ClustersQueryDone,
                       weak_ptr_factory_.GetWeakPtr(), env,
                       ScopedJavaGlobalRef<jobject>(j_this),
                       ScopedJavaGlobalRef<jobject>(j_callback));
    query_clusters_state_->LoadNextBatchOfClusters(std::move(callback));
  }
}

void HistoryClustersBridge::ClustersQueryDone(
    JNIEnv* env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jobject>& j_callback,
    const std::string& query,
    std::vector<history::Cluster> clusters,
    bool can_load_more,
    bool is_continuation) {
  std::vector<ScopedJavaLocalRef<jobject>> j_clusters;
  for (const history::Cluster& cluster : clusters) {
    std::vector<ScopedJavaLocalRef<jobject>> cluster_visits;
    for (const history::ClusterVisit& visit : cluster.visits) {
      const ScopedJavaLocalRef<jobject>& j_cluster_visit =
          Java_HistoryClustersBridge_buildClusterVisit(
              env, visit.score,
              url::GURLAndroid::FromNativeGURL(env, visit.normalized_url),
              base::android::ConvertUTF16ToJavaString(
                  env, visit.annotated_visit.url_row.title()));
      cluster_visits.push_back(j_cluster_visit);
    }
    ScopedJavaLocalRef<jclass> cluster_visit_type = base::android::GetClass(
        env, "org/chromium/chrome/browser/history_clusters/ClusterVisit");
    std::u16string label = cluster.label.value_or(u"no_label");
    // Passing objects more complex than primitives requires extra JNI hops, so
    // we destructure matches into arrays which can be passed in one hop.
    std::vector<int> label_match_starts;
    std::vector<int> label_match_ends;
    for (const auto& match : cluster.label_match_positions) {
      label_match_starts.push_back(match.first);
      label_match_ends.push_back(match.second);
    }

    const ScopedJavaLocalRef<jobject>& j_cluster =
        Java_HistoryClustersBridge_buildCluster(
            env,
            base::android::ToTypedJavaArrayOfObjects(env, cluster_visits,
                                                     cluster_visit_type),
            base::android::ToJavaArrayOfStrings(env, cluster.keywords),
            base::android::ConvertUTF16ToJavaString(env, label),
            base::android::ToJavaIntArray(env, label_match_starts),
            base::android::ToJavaIntArray(env, label_match_ends));
    j_clusters.push_back(j_cluster);
  }
  ScopedJavaLocalRef<jclass> cluster_type = base::android::GetClass(
      env, "org/chromium/chrome/browser/history_clusters/HistoryCluster");
  const ScopedJavaLocalRef<jobject>& j_result =
      Java_HistoryClustersBridge_buildClusterResult(
          env,
          base::android::ToTypedJavaArrayOfObjects(env, j_clusters,
                                                   cluster_type),
          base::android::ConvertUTF8ToJavaString(env, query), can_load_more,
          is_continuation);
  base::android::RunObjectCallbackAndroid(j_callback, j_result);
}

void HistoryClustersBridge::Destroy(JNIEnv* j_env) {
  delete this;
}

}  // namespace history_clusters

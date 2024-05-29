// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_bridge.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_types.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/history_clusters/jni_headers/HistoryClustersBridge_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {
const char kHistoryClustersBridgeKey[] = "history-clusters-bridge";
}

namespace history_clusters {

HistoryClustersBridge::HistoryClustersBridge(
    JNIEnv* env,
    HistoryClustersService* history_clusters_service)
    : history_clusters_service_(history_clusters_service) {
  ScopedJavaLocalRef<jobject> j_history_clusters_bridge =
      Java_HistoryClustersBridge_create(env, reinterpret_cast<jlong>(this));
  java_ref_.Reset(j_history_clusters_bridge);
}

HistoryClustersBridge::~HistoryClustersBridge() = default;

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
      std::vector<int> title_match_starts;
      std::vector<int> title_match_ends;
      for (const auto& match : visit.title_match_positions) {
        title_match_starts.push_back(match.first);
        title_match_ends.push_back(match.second);
      }

      std::vector<int> url_match_starts;
      std::vector<int> url_match_ends;
      for (const auto& match : visit.url_for_display_match_positions) {
        url_match_starts.push_back(match.first);
        url_match_ends.push_back(match.second);
      }

      std::vector<int64_t> duplicated_visit_timestamps;
      std::vector<ScopedJavaLocalRef<jobject>> duplicated_visit_urls;
      for (const auto& duplicate : visit.duplicate_visits) {
        duplicated_visit_timestamps.push_back(
            duplicate.visit_time.ToInternalValue());
        duplicated_visit_urls.push_back(
            url::GURLAndroid::FromNativeGURL(env, duplicate.url));
      }

      const ScopedJavaLocalRef<jobject>& j_cluster_visit =
          Java_HistoryClustersBridge_buildClusterVisit(
              env, visit.score,
              url::GURLAndroid::FromNativeGURL(env, visit.normalized_url),
              base::android::ConvertUTF16ToJavaString(env,
                                                      visit.url_for_display),
              base::android::ConvertUTF16ToJavaString(
                  env, visit.annotated_visit.url_row.title()),
              base::android::ToJavaIntArray(env, title_match_starts),
              base::android::ToJavaIntArray(env, title_match_ends),
              base::android::ToJavaIntArray(env, url_match_starts),
              base::android::ToJavaIntArray(env, url_match_ends),
              url::GURLAndroid::FromNativeGURL(
                  env, visit.annotated_visit.url_row.url()),
              visit.annotated_visit.visit_row.visit_time.ToInternalValue(),
              base::android::ToJavaLongArray(env, duplicated_visit_timestamps),
              duplicated_visit_urls);
      cluster_visits.push_back(j_cluster_visit);
    }
    base::Time visit_time;
    if (!cluster.visits.empty())
      visit_time = cluster.visits[0].annotated_visit.visit_row.visit_time;
    ScopedJavaLocalRef<jclass> cluster_visit_type = base::android::GetClass(
        env, "org/chromium/chrome/browser/history_clusters/ClusterVisit");
    std::u16string label = cluster.label.value_or(u"no_label");
    std::u16string raw_label = cluster.raw_label.value_or(u"no_label");

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
            base::android::ConvertUTF16ToJavaString(env, label),
            base::android::ConvertUTF16ToJavaString(env, raw_label),
            base::android::ToJavaIntArray(env, label_match_starts),
            base::android::ToJavaIntArray(env, label_match_ends),
            visit_time.InMillisecondsSinceUnixEpoch(),
            base::android::ToJavaArrayOfStrings(env, cluster.related_searches));
    j_clusters.push_back(j_cluster);
  }
  ScopedJavaLocalRef<jclass> cluster_type = base::android::GetClass(
      env, "org/chromium/chrome/browser/history_clusters/HistoryCluster");
  std::vector<std::u16string> unique_raw_labels;
  std::vector<int> label_counts;
  if (query_clusters_state_->query().empty()) {
    for (const auto& label_entry :
         query_clusters_state_->raw_label_counts_so_far()) {
      unique_raw_labels.push_back(label_entry.first);
      label_counts.push_back(label_entry.second);
    }
  }

  const ScopedJavaLocalRef<jobject>& j_result =
      Java_HistoryClustersBridge_buildClusterResult(
          env,
          base::android::ToTypedJavaArrayOfObjects(env, j_clusters,
                                                   cluster_type),
          base::android::ToJavaArrayOfStrings(env, unique_raw_labels),
          base::android::ToJavaIntArray(env, label_counts),
          base::android::ConvertUTF8ToJavaString(env, query), can_load_more,
          is_continuation);
  base::android::RunObjectCallbackAndroid(j_callback, j_result);
}

void HistoryClustersBridge::Destroy(JNIEnv* j_env) {
  delete this;
}

}  // namespace history_clusters

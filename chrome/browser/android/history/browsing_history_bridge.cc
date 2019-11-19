// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history/browsing_history_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/BrowsingHistoryBridge_jni.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/url_formatter/url_formatter.h"

using history::BrowsingHistoryService;

const int kMaxQueryCount = 150;

BrowsingHistoryBridge::BrowsingHistoryBridge(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             bool is_incognito) {
  Profile* last_profile = ProfileManager::GetLastUsedProfile();
  // We cannot trust GetLastUsedProfile() to return the original or incognito
  // profile. Instead the boolean |is_incognito| is passed to track this choice.
  // As of writing GetLastUsedProfile() will always return the original profile,
  // but to be more defensive, manually grab the original profile anyway. Note
  // that while some platforms might not open history when incognito, Android
  // does, but only shows local history.
  profile_ = is_incognito ? last_profile->GetOffTheRecordProfile()
                          : last_profile->GetOriginalProfile();

  history::HistoryService* local_history = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  browsing_history_service_ = std::make_unique<BrowsingHistoryService>(
      this, local_history, sync_service);

  j_history_service_obj_.Reset(env, obj);
}

BrowsingHistoryBridge::~BrowsingHistoryBridge() {}

void BrowsingHistoryBridge::Destroy(JNIEnv*, const JavaParamRef<jobject>&) {
  delete this;
}

void BrowsingHistoryBridge::QueryHistory(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj,
    jstring j_query) {
  j_query_result_obj_.Reset(env, j_result_obj);
  query_history_continuation_.Reset();

  history::QueryOptions options;
  options.max_count = kMaxQueryCount;
  options.duplicate_policy = history::QueryOptions::REMOVE_DUPLICATES_PER_DAY;

  browsing_history_service_->QueryHistory(
      base::android::ConvertJavaStringToUTF16(env, j_query), options);
}

void BrowsingHistoryBridge::QueryHistoryContinuation(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(query_history_continuation_);
  j_query_result_obj_.Reset(env, j_result_obj);
  std::move(query_history_continuation_).Run();
}

void BrowsingHistoryBridge::OnQueryComplete(
    const std::vector<BrowsingHistoryService::HistoryEntry>& results,
    const BrowsingHistoryService::QueryResultsInfo& query_results_info,
    base::OnceClosure continuation_closure) {
  JNIEnv* env = base::android::AttachCurrentThread();
  query_history_continuation_ = std::move(continuation_closure);

  for (const BrowsingHistoryService::HistoryEntry& entry : results) {
    // TODO(twellington): Move the domain logic to BrowsingHistoryServce so it
    // can be shared with ContentBrowsingHistoryDriver.
    base::string16 domain = url_formatter::IDNToUnicode(entry.url.host());
    // When the domain is empty, use the scheme instead. This allows for a
    // sensible treatment of e.g. file: URLs when group by domain is on.
    if (domain.empty())
      domain = base::UTF8ToUTF16(entry.url.scheme() + ":");

    // This relies on |all_timestamps| being a sorted data structure.
    int64_t most_recent_java_timestamp =
        base::Time::FromInternalValue(*entry.all_timestamps.rbegin())
            .ToJavaTime();
    std::vector<int64_t> native_timestamps(entry.all_timestamps.begin(),
                                           entry.all_timestamps.end());

    Java_BrowsingHistoryBridge_createHistoryItemAndAddToList(
        env, j_query_result_obj_,
        base::android::ConvertUTF8ToJavaString(env, entry.url.spec()),
        base::android::ConvertUTF16ToJavaString(env, domain),
        base::android::ConvertUTF16ToJavaString(env, entry.title),
        most_recent_java_timestamp,
        base::android::ToJavaLongArray(env, native_timestamps),
        entry.blocked_visit);
  }

  Java_BrowsingHistoryBridge_onQueryHistoryComplete(
      env, j_history_service_obj_, j_query_result_obj_,
      !(query_results_info.reached_beginning));
}

void BrowsingHistoryBridge::MarkItemForRemoval(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jstring j_url,
    const JavaParamRef<jlongArray>& j_native_timestamps) {
  BrowsingHistoryService::HistoryEntry entry;
  entry.url = GURL(base::android::ConvertJavaStringToUTF16(env, j_url));

  std::vector<int64_t> timestamps;
  base::android::JavaLongArrayToInt64Vector(env, j_native_timestamps,
                                            &timestamps);
  entry.all_timestamps.insert(timestamps.begin(), timestamps.end());

  items_to_remove_.push_back(entry);
}

void BrowsingHistoryBridge::RemoveItems(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  browsing_history_service_->RemoveVisits(items_to_remove_);
  items_to_remove_.clear();
}

void BrowsingHistoryBridge::OnRemoveVisitsComplete() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowsingHistoryBridge_onRemoveComplete(env, j_history_service_obj_);
}

void BrowsingHistoryBridge::OnRemoveVisitsFailed() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowsingHistoryBridge_onRemoveFailed(env, j_history_service_obj_);
}

void BrowsingHistoryBridge::HistoryDeleted() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowsingHistoryBridge_onHistoryDeleted(env, j_history_service_obj_);
}

void BrowsingHistoryBridge::HasOtherFormsOfBrowsingHistory(
    bool has_other_forms, bool has_synced_results) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowsingHistoryBridge_hasOtherFormsOfBrowsingData(
      env, j_history_service_obj_, has_other_forms);
}

Profile* BrowsingHistoryBridge::GetProfile() {
  return profile_;
}

static jlong JNI_BrowsingHistoryBridge_Init(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            jboolean is_incognito) {
  BrowsingHistoryBridge* bridge =
      new BrowsingHistoryBridge(env, obj, is_incognito);
  return reinterpret_cast<intptr_t>(bridge);
}

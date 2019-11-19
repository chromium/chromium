// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_bridge.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/UsageStatsBridge_jni.h"
#include "chrome/browser/android/usage_stats/usage_stats_database.h"
#include "chrome/browser/android/usage_stats/website_event.pb.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/pref_registry/pref_registry_syncable.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfByteArray;
using base::android::ToJavaArrayOfStrings;

namespace usage_stats {

namespace {

bool isSuccess(UsageStatsDatabase::Error error) {
  return error == UsageStatsDatabase::Error::kNoError;
}

}  // namespace

static jlong JNI_UsageStatsBridge_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& j_this,
                                       const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);

  std::unique_ptr<UsageStatsDatabase> usage_stats_database =
      std::make_unique<UsageStatsDatabase>(profile);

  UsageStatsBridge* native_usage_stats_bridge =
      new UsageStatsBridge(std::move(usage_stats_database), profile, j_this);

  return reinterpret_cast<intptr_t>(native_usage_stats_bridge);
}

UsageStatsBridge::UsageStatsBridge(
    std::unique_ptr<UsageStatsDatabase> usage_stats_database,
    Profile* profile,
    const JavaRef<jobject>& j_this)
    : usage_stats_database_(std::move(usage_stats_database)),
      profile_(profile),
      j_this_(ScopedJavaGlobalRef<jobject>(j_this)) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (history_service)
    history_service->AddObserver(this);
}

UsageStatsBridge::~UsageStatsBridge() {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (history_service)
    history_service->RemoveObserver(this);
}

void UsageStatsBridge::Destroy(JNIEnv* env, const JavaRef<jobject>& j_this) {
  delete this;
}

void UsageStatsBridge::GetAllEvents(JNIEnv* j_env,
                                    const JavaRef<jobject>& j_this,
                                    const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->GetAllEvents(
      base::BindOnce(&UsageStatsBridge::OnGetEventsDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::QueryEventsInRange(JNIEnv* j_env,
                                          const JavaRef<jobject>& j_this,
                                          const jlong j_start,
                                          const jlong j_end,
                                          const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->QueryEventsInRange(
      base::Time::FromJavaTime(j_start), base::Time::FromJavaTime(j_end),
      base::BindOnce(&UsageStatsBridge::OnGetEventsDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::AddEvents(JNIEnv* j_env,
                                 const JavaRef<jobject>& j_this,
                                 const JavaRef<jobjectArray>& j_events,
                                 const JavaRef<jobject>& j_callback) {
  // Deserialize events from byte arrays to proto messages.
  std::vector<std::string> serialized_events;
  JavaArrayOfByteArrayToStringVector(j_env, j_events, &serialized_events);

  std::vector<WebsiteEvent> events;
  events.reserve(serialized_events.size());

  for (const std::string& serialized : serialized_events) {
    WebsiteEvent event;
    event.ParseFromString(serialized);
    events.emplace_back(event);
  }

  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->AddEvents(
      events, base::BindOnce(&UsageStatsBridge::OnUpdateDone,
                             weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::DeleteAllEvents(JNIEnv* j_env,
                                       const JavaRef<jobject>& j_this,
                                       const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->DeleteAllEvents(
      base::BindOnce(&UsageStatsBridge::OnUpdateDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::DeleteEventsInRange(JNIEnv* j_env,
                                           const JavaRef<jobject>& j_this,
                                           const jlong j_start,
                                           const jlong j_end,
                                           const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->DeleteEventsInRange(
      base::Time::FromJavaTime(j_start), base::Time::FromJavaTime(j_end),
      base::BindOnce(&UsageStatsBridge::OnUpdateDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::DeleteEventsWithMatchingDomains(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jobjectArray>& j_domains,
    const JavaRef<jobject>& j_callback) {
  std::vector<std::string> domains;
  AppendJavaStringArrayToStringVector(j_env, j_domains, &domains);

  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->DeleteEventsWithMatchingDomains(
      base::flat_set<std::string>(domains),
      base::BindOnce(&UsageStatsBridge::OnUpdateDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::GetAllSuspensions(JNIEnv* j_env,
                                         const JavaRef<jobject>& j_this,
                                         const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->GetAllSuspensions(
      base::BindOnce(&UsageStatsBridge::OnGetAllSuspensionsDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::SetSuspensions(JNIEnv* j_env,
                                      const JavaRef<jobject>& j_this,
                                      const JavaRef<jobjectArray>& j_domains,
                                      const JavaRef<jobject>& j_callback) {
  std::vector<std::string> domains;
  AppendJavaStringArrayToStringVector(j_env, j_domains, &domains);

  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->SetSuspensions(
      domains, base::BindOnce(&UsageStatsBridge::OnUpdateDone,
                              weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::GetAllTokenMappings(JNIEnv* j_env,
                                           const JavaRef<jobject>& j_this,
                                           const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  usage_stats_database_->GetAllTokenMappings(
      base::BindOnce(&UsageStatsBridge::OnGetAllTokenMappingsDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::SetTokenMappings(JNIEnv* j_env,
                                        const JavaRef<jobject>& j_this,
                                        const JavaRef<jobjectArray>& j_tokens,
                                        const JavaRef<jobjectArray>& j_fqdns,
                                        const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  std::vector<std::string> tokens, fqdns;
  AppendJavaStringArrayToStringVector(j_env, j_tokens, &tokens);
  AppendJavaStringArrayToStringVector(j_env, j_fqdns, &fqdns);

  DCHECK(tokens.size() == fqdns.size());

  // Zip tokens (keys) and FQDNs (values) into a map.
  UsageStatsDatabase::TokenMap mappings;
  for (size_t i = 0; i < tokens.size(); i++) {
    mappings.emplace(tokens[i], fqdns[i]);
  }

  usage_stats_database_->SetTokenMappings(
      mappings, base::BindOnce(&UsageStatsBridge::OnUpdateDone,
                               weak_ptr_factory_.GetWeakPtr(), callback));
}

void UsageStatsBridge::OnGetEventsDone(ScopedJavaGlobalRef<jobject> callback,
                                       UsageStatsDatabase::Error error,
                                       std::vector<WebsiteEvent> events) {
  JNIEnv* env = AttachCurrentThread();

  if (!isSuccess(error)) {
    Java_UsageStatsBridge_createEventListAndRunCallback(
        env, ToJavaArrayOfByteArray(env, std::vector<std::string>()), callback);
    return;
  }

  // Serialize WebsiteEvent proto messages for passing over JNI bridge as byte
  // arrays.
  std::vector<std::string> serialized_events;
  serialized_events.reserve(events.size());

  for (WebsiteEvent event : events) {
    std::string serialized;
    event.SerializeToString(&serialized);
    serialized_events.emplace_back(serialized);
  }

  ScopedJavaLocalRef<jobjectArray> j_serialized_events =
      ToJavaArrayOfByteArray(env, serialized_events);

  // Over JNI, deserialize to list of WebsiteEvent messages, and run on given
  // callback.
  Java_UsageStatsBridge_createEventListAndRunCallback(env, j_serialized_events,
                                                      callback);
}

void UsageStatsBridge::OnGetAllSuspensionsDone(
    ScopedJavaGlobalRef<jobject> callback,
    UsageStatsDatabase::Error error,
    std::vector<std::string> suspensions) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> j_suspensions =
      isSuccess(error) ? ToJavaArrayOfStrings(env, suspensions)
                       : ToJavaArrayOfStrings(env, std::vector<std::string>());

  RunObjectCallbackAndroid(callback, j_suspensions);
}

void UsageStatsBridge::OnGetAllTokenMappingsDone(
    ScopedJavaGlobalRef<jobject> callback,
    UsageStatsDatabase::Error error,
    UsageStatsDatabase::TokenMap mappings) {
  JNIEnv* env = AttachCurrentThread();

  if (!isSuccess(error)) {
    Java_UsageStatsBridge_createMapAndRunCallback(
        env, ToJavaArrayOfStrings(env, std::vector<std::string>()),
        ToJavaArrayOfStrings(env, std::vector<std::string>()), callback);
    return;
  }

  // Create separate vectors of keys and values from map for passing over
  // JNI bridge as String arrays.
  std::vector<std::string> keys, values;
  keys.reserve(mappings.size());
  values.reserve(mappings.size());

  for (auto mapping : mappings) {
    keys.emplace_back(std::move(mapping.first));
    values.emplace_back(std::move(mapping.second));
  }

  ScopedJavaLocalRef<jobjectArray> j_keys = ToJavaArrayOfStrings(env, keys);
  ScopedJavaLocalRef<jobjectArray> j_values = ToJavaArrayOfStrings(env, values);

  // Over JNI, reconstruct map from keys and values, and run on given callback.
  Java_UsageStatsBridge_createMapAndRunCallback(env, j_keys, j_values,
                                                callback);
}

void UsageStatsBridge::OnUpdateDone(ScopedJavaGlobalRef<jobject> callback,
                                    UsageStatsDatabase::Error error) {
  RunBooleanCallbackAndroid(callback, isSuccess(error));
}

// static
void UsageStatsBridge::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kUsageStatsEnabled, false);
}

void UsageStatsBridge::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // We ignore expirations since they're not user-initiated.
  if (deletion_info.is_from_expiration()) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();

  if (deletion_info.IsAllHistory()) {
    Java_UsageStatsBridge_onAllHistoryDeleted(env, j_this_);
    return;
  }

  history::DeletionTimeRange time_range = deletion_info.time_range();
  if (time_range.IsValid()) {
    const base::Optional<std::set<GURL>>& urls = deletion_info.restrict_urls();
    if (urls.has_value() && urls.value().size() > 0) {
      std::vector<std::string> domains;
      domains.reserve(urls.value().size());
      for (const auto& gurl : urls.value()) {
        domains.push_back(gurl.host());
      }
      Java_UsageStatsBridge_onHistoryDeletedForDomains(
          env, j_this_, ToJavaArrayOfStrings(env, domains));
    } else {
      int64_t startTimeMs = time_range.begin().ToJavaTime();
      int64_t endTimeMs = time_range.end().ToJavaTime();

      Java_UsageStatsBridge_onHistoryDeletedInRange(env, j_this_, startTimeMs,
                                                    endTimeMs);
    }

    return;
  }
}

}  // namespace usage_stats

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/android/usage_stats/usage_stats_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

namespace history {
class HistoryService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace usage_stats {

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

/* Native counterpart of UsageStatsBridge.java. Holds non-owning pointers to
 * native implementation to which operations are delegated. This bridge is
 * instantiated, owned, and destroyed from Java.
 */
class UsageStatsBridge : public history::HistoryServiceObserver {
 public:
  explicit UsageStatsBridge(
      std::unique_ptr<UsageStatsDatabase> usage_stats_database,
      Profile* profile,
      const JavaRef<jobject>& j_this);

  UsageStatsBridge(const UsageStatsBridge&) = delete;
  UsageStatsBridge& operator=(const UsageStatsBridge&) = delete;

  ~UsageStatsBridge() override;

  void Destroy(JNIEnv* j_env, const JavaRef<jobject>& j_this);

  void GetAllEvents(JNIEnv* j_env,
                    const JavaRef<jobject>& j_this,
                    const JavaRef<jobject>& j_callback);

  void QueryEventsInRange(JNIEnv* j_env,
                          const JavaRef<jobject>& j_this,
                          const jlong j_start,
                          const jlong j_end,
                          const JavaRef<jobject>& j_callback);

  void AddEvents(JNIEnv* j_env,
                 const JavaRef<jobject>& j_this,
                 const JavaRef<jobjectArray>& j_events,
                 const JavaRef<jobject>& j_callback);

  void DeleteAllEvents(JNIEnv* j_env,
                       const JavaRef<jobject>& j_this,
                       const JavaRef<jobject>& j_callback);

  void DeleteEventsInRange(JNIEnv* j_env,
                           const JavaRef<jobject>& j_this,
                           const jlong j_start,
                           const jlong j_end,
                           const JavaRef<jobject>& j_callback);

  void DeleteEventsWithMatchingDomains(JNIEnv* j_env,
                                       const JavaRef<jobject>& j_this,
                                       const JavaRef<jobjectArray>& j_domains,
                                       const JavaRef<jobject>& j_callback);

  void GetAllSuspensions(JNIEnv* j_env,
                         const JavaRef<jobject>& j_this,
                         const JavaRef<jobject>& j_callback);

  void SetSuspensions(JNIEnv* j_env,
                      const JavaRef<jobject>& j_this,
                      const JavaRef<jobjectArray>& j_domains,
                      const JavaRef<jobject>& j_callback);

  void GetAllTokenMappings(JNIEnv* j_env,
                           const JavaRef<jobject>& j_this,
                           const JavaRef<jobject>& j_callback);

  void SetTokenMappings(JNIEnv* j_env,
                        const JavaRef<jobject>& j_this,
                        const JavaRef<jobjectArray>& j_tokens,
                        const JavaRef<jobjectArray>& j_fqdns,
                        const JavaRef<jobject>& j_callback);

  // Overridden from history::HistoryServiceObserver.
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  void OnGetEventsDone(ScopedJavaGlobalRef<jobject> callback,
                       UsageStatsDatabase::Error error,
                       std::vector<WebsiteEvent> events);

  void OnGetAllSuspensionsDone(ScopedJavaGlobalRef<jobject> callback,
                               UsageStatsDatabase::Error error,
                               std::vector<std::string> suspensions);

  void OnGetAllTokenMappingsDone(ScopedJavaGlobalRef<jobject> callback,
                                 UsageStatsDatabase::Error error,
                                 UsageStatsDatabase::TokenMap mappings);

  void OnUpdateDone(ScopedJavaGlobalRef<jobject> callback,
                    UsageStatsDatabase::Error error);

  std::unique_ptr<UsageStatsDatabase> usage_stats_database_;

  raw_ptr<Profile> profile_;

  base::android::ScopedJavaGlobalRef<jobject> j_this_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      scoped_history_service_observer_{this};

  base::WeakPtrFactory<UsageStatsBridge> weak_ptr_factory_{this};
};

}  // namespace usage_stats

#endif  // CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_BRIDGE_H_

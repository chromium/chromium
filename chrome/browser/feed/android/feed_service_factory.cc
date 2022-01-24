// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/android/feed_service_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feed/android/feed_service_bridge.h"
#include "chrome/browser/feed/android/refresh_task_scheduler_impl.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_version.h"
#include "components/background_task_scheduler/background_task_scheduler_factory.h"
#include "components/feed/buildflags.h"
#include "components/feed/core/proto/v2/keyvalue_store.pb.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"

namespace feed {
const char kFeedv2Folder[] = "feedv2";
namespace internal {
const base::StringPiece GetFollowingFeedFollowCountGroupName(
    size_t follow_count) {
  if (follow_count == 0)
    return "None";
  if (follow_count <= 4)
    return "1-4";
  if (follow_count <= 8)
    return "5-8";
  if (follow_count <= 12)
    return "9-12";
  if (follow_count <= 20)
    return "13-20";
  return "21+";
}
}  // namespace internal

class FeedServiceDelegateImpl : public FeedService::Delegate {
 public:
  ~FeedServiceDelegateImpl() override = default;
  std::string GetLanguageTag() override {
    return FeedServiceBridge::GetLanguageTag();
  }
  DisplayMetrics GetDisplayMetrics() override {
    return FeedServiceBridge::GetDisplayMetrics();
  }
  bool IsAutoplayEnabled() override {
    return FeedServiceBridge::IsAutoplayEnabled();
  }
  void ClearAll() override { FeedServiceBridge::ClearAll(); }
  void PrefetchImage(const GURL& url) override {
    FeedServiceBridge::PrefetchImage(url);
  }
  void RegisterExperiments(const Experiments& experiments) override {
    // Note that this does not affect the contents of the X-Client-Data
    // by design. We do not provide the variations IDs from the backend
    // and do not attach them to the X-Client-Data header.
    for (const auto& exp : experiments) {
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(exp.first,
                                                                exp.second);
    }
  }
  void RegisterFollowingFeedFollowCountFieldTrial(
      size_t follow_count) override {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "FollowingFeedFollowCount",
        internal::GetFollowingFeedFollowCountGroupName(follow_count));
  }
};

// static
FeedService* FeedServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
// Note that if both v1 and v2 are disabled in the build, feed::IsV2Enabled()
// returns true. In that case, this function will return null. This prevents
// creation of the Feed surface from triggering any other Feed behavior.
#if BUILDFLAG(ENABLE_FEED_V2)
  if (context)
    return static_cast<FeedService*>(
        GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
  return nullptr;
#else
  return nullptr;
#endif
}

// static
FeedServiceFactory* FeedServiceFactory::GetInstance() {
  return base::Singleton<FeedServiceFactory>::get();
}

FeedServiceFactory::FeedServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FeedService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(background_task::BackgroundTaskSchedulerFactory::GetInstance());
}

FeedServiceFactory::~FeedServiceFactory() = default;

KeyedService* FeedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  content::StoragePartition* storage_partition =
      context->GetDefaultStoragePartition();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::string api_key;
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    bool is_stable_channel =
        chrome::GetChannel() == version_info::Channel::STABLE;
    api_key = is_stable_channel ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});

  base::FilePath feed_dir(profile->GetPath().Append(kFeedv2Folder));

  feed::ChromeInfo chrome_info;
  chrome_info.version = base::Version({CHROME_VERSION});
  chrome_info.channel = chrome::GetChannel();
  chrome_info.start_surface =
      base::FeatureList::IsEnabled(chrome::android::kStartSurfaceAndroid);

  return new FeedService(
      std::make_unique<FeedServiceDelegateImpl>(),
      std::make_unique<RefreshTaskSchedulerImpl>(
          background_task::BackgroundTaskSchedulerFactory::GetForKey(
              profile->GetProfileKey())),
      profile->GetPrefs(), g_browser_process->local_state(),
      storage_partition->GetProtoDatabaseProvider()->GetDB<feedstore::Record>(
          leveldb_proto::ProtoDbType::FEED_STREAM_DATABASE,
          feed_dir.AppendASCII("streamdb"), background_task_runner),
      storage_partition->GetProtoDatabaseProvider()->GetDB<feedkvstore::Entry>(
          leveldb_proto::ProtoDbType::FEED_KEY_VALUE_DATABASE,
          feed_dir.AppendASCII("keyvaldb"), background_task_runner),
      identity_manager,
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS),
      storage_partition->GetURLLoaderFactoryForBrowserProcess(),
      background_task_runner, api_key, chrome_info);
}

content::BrowserContext* FeedServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context->IsOffTheRecord() ? nullptr : context;
}

bool FeedServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace feed

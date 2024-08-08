// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/feed_service_factory.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_version.h"
#include "components/background_task_scheduler/background_task_scheduler_factory.h"
#include "components/feed/core/proto/v2/keyvalue_store.pb.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/service/variations_service_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/feed/android/feed_service_bridge.h"
#include "chrome/browser/feed/android/refresh_task_scheduler_impl.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

namespace feed {
const base::FilePath::CharType kFeedv2Folder[] = FILE_PATH_LITERAL("feedv2");

namespace internal {
const std::string_view GetFollowingFeedFollowCountGroupName(
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

#if !BUILDFLAG(IS_ANDROID)
// TODO(jianli): Need to figure out what to do for desktop version.
class NoOpRefreshTaskScheduler : public feed::RefreshTaskScheduler {
 public:
  NoOpRefreshTaskScheduler() = default;
  ~NoOpRefreshTaskScheduler() override = default;

  void EnsureScheduled(RefreshTaskId id, base::TimeDelta delay) override {}
  void Cancel(RefreshTaskId id) override {}
  void RefreshTaskComplete(RefreshTaskId id) override {}
};
#endif

}  // namespace internal

class FeedServiceDelegateImpl : public FeedService::Delegate {
 public:
  ~FeedServiceDelegateImpl() override = default;
  std::string GetLanguageTag() override {
#if BUILDFLAG(IS_ANDROID)
    return FeedServiceBridge::GetLanguageTag();
#else
    // TODO(jianli): Need to figure out what to do for desktop version.
    return "en";
#endif
  }
  std::string GetCountry() override { return FeedServiceFactory::GetCountry(); }
  DisplayMetrics GetDisplayMetrics() override {
#if BUILDFLAG(IS_ANDROID)
    return FeedServiceBridge::GetDisplayMetrics();
#else
    // TODO(jianli): Need to figure out what to do for desktop version.
    DisplayMetrics metrics;
    metrics.density = 0;
    metrics.width_pixels = 0;
    metrics.height_pixels = 0;
    return metrics;
#endif
  }
  TabGroupEnabledState GetTabGroupEnabledState() override {
#if BUILDFLAG(IS_ANDROID)
    return TabGroupEnabledState::kBoth;
#else
    return TabGroupEnabledState::kNone;
#endif
  }
  void ClearAll() override {
    // TODO(jianli): Need to figure out what to do for desktop version.
#if BUILDFLAG(IS_ANDROID)
    FeedServiceBridge::ClearAll();
#endif
  }
  void PrefetchImage(const GURL& url) override {
    // TODO(jianli): Need to figure out what to do for desktop version.
#if BUILDFLAG(IS_ANDROID)
    FeedServiceBridge::PrefetchImage(url);
#endif
  }
  void RegisterExperiments(const Experiments& experiments) override {
    experiments_ = experiments;

    // Note that this does not affect the contents of the X-Client-Data
    // by design. We do not provide the variations IDs from the backend
    // and do not attach them to the X-Client-Data header.
    for (const auto& exp : experiments) {
      for (const auto& group : exp.second) {
        ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(exp.first,
                                                                  group.name);
      }
    }
  }
  void RegisterFollowingFeedFollowCountFieldTrial(
      size_t follow_count) override {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "FollowingFeedFollowCount",
        internal::GetFollowingFeedFollowCountGroupName(follow_count));
  }
  void RegisterFeedUserSettingsFieldTrial(std::string_view group) override {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "FeedUserSettings", group);
  }
  const Experiments& GetExperiments() const override { return experiments_; }

 private:
  Experiments experiments_;
};

// static
FeedService* FeedServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
// Note that if both v1 and v2 are disabled in the build, feed::IsV2Enabled()
// returns true. In that case, this function will return null. This prevents
// creation of the Feed surface from triggering any other Feed behavior.
  if (context)
    return static_cast<FeedService*>(
        GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
  return nullptr;
}

// static
FeedServiceFactory* FeedServiceFactory::GetInstance() {
  static base::NoDestructor<FeedServiceFactory> instance;
  return instance.get();
}

// static
std::string FeedServiceFactory::GetCountry() {
  return base::ToUpperASCII(variations::GetCurrentCountryCode(
      g_browser_process->variations_service()));
}

FeedServiceFactory::FeedServiceFactory()
    : ProfileKeyedServiceFactory(
          "FeedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(background_task::BackgroundTaskSchedulerFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

FeedServiceFactory::~FeedServiceFactory() = default;

std::unique_ptr<KeyedService>
FeedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  content::StoragePartition* storage_partition =
      context->GetDefaultStoragePartition();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::string api_key;
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    api_key = google_apis::GetAPIKey(chrome::GetChannel());
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});

  base::FilePath feed_dir(profile->GetPath().Append(kFeedv2Folder));

  feed::ChromeInfo chrome_info;
  chrome_info.version = base::Version({CHROME_VERSION});
  chrome_info.channel = chrome::GetChannel();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
#if BUILDFLAG(IS_ANDROID)
  chrome_info.is_new_tab_search_engine_url_android_enabled =
      template_url_service->IsEeaChoiceCountry();
#else
  chrome_info.is_new_tab_search_engine_url_android_enabled = false;
#endif

  return std::make_unique<FeedService>(
      std::make_unique<FeedServiceDelegateImpl>(),
#if BUILDFLAG(IS_ANDROID)
      std::make_unique<RefreshTaskSchedulerImpl>(
          background_task::BackgroundTaskSchedulerFactory::GetForKey(
              profile->GetProfileKey())),
#else
      std::make_unique<internal::NoOpRefreshTaskScheduler>(),
#endif
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
      background_task_runner, api_key, chrome_info, template_url_service);
}

bool FeedServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace feed

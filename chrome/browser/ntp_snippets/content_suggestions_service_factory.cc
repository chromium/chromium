// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/prefetched_pages_tracker.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_status_service_impl.h"
#include "components/ntp_snippets/remote/request_params.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/ntp/ntp_snippets_launcher.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/feed/feed_feature_list.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "components/ntp_snippets/remote/prefetched_pages_tracker_impl.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#endif

using content::BrowserThread;
using history::HistoryService;
using image_fetcher::ImageFetcherImpl;
using language::UrlLanguageHistogram;
using ntp_snippets::CategoryRanker;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::GetFetchEndpoint;
using ntp_snippets::PersistentScheduler;
using ntp_snippets::PrefetchedPagesTracker;
using ntp_snippets::RemoteSuggestionsDatabase;
using ntp_snippets::RemoteSuggestionsFetcherImpl;
using ntp_snippets::RemoteSuggestionsProviderImpl;
using ntp_snippets::RemoteSuggestionsSchedulerImpl;
using ntp_snippets::RemoteSuggestionsStatusServiceImpl;
using ntp_snippets::UserClassifier;

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
using ntp_snippets::PrefetchedPagesTrackerImpl;
using offline_pages::OfflinePageModel;
using offline_pages::OfflinePageModelFactory;
using offline_pages::RequestCoordinator;
using offline_pages::RequestCoordinatorFactory;
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

// For now, ContentSuggestionsService must only be instantiated on Android.
// See also crbug.com/688366.
#if defined(OS_ANDROID)
#define CONTENT_SUGGESTIONS_ENABLED 1
#else
#define CONTENT_SUGGESTIONS_ENABLED 0
#endif  // OS_ANDROID

// The actual #if that does the work is below in BuildServiceInstanceFor. This
// one is just required to avoid "unused code" compiler errors.
#if CONTENT_SUGGESTIONS_ENABLED

namespace {

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)

void RegisterWithPrefetching(ContentSuggestionsService* service,
                             Profile* profile) {
  // There's a circular dependency between ContentSuggestionsService and
  // PrefetchService. This closes the circle.
  offline_pages::PrefetchServiceFactory::GetForKey(profile->GetProfileKey())
      ->SetContentSuggestionsService(service);
}

#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

bool IsArticleProviderEnabled() {
  return base::FeatureList::IsEnabled(ntp_snippets::kArticleSuggestionsFeature);
}

bool IsKeepingPrefetchedSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(
      ntp_snippets::kKeepPrefetchedContentSuggestions);
}

void ParseJson(const std::string& json,
               ntp_snippets::SuccessCallback success_callback,
               ntp_snippets::ErrorCallback error_callback) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      json, base::BindOnce(
                [](ntp_snippets::SuccessCallback success_callback,
                   ntp_snippets::ErrorCallback error_callback,
                   data_decoder::DataDecoder::ValueOrError result) {
                  if (!result.value)
                    std::move(error_callback).Run(*result.error);
                  else
                    std::move(success_callback).Run(std::move(*result.value));
                },
                std::move(success_callback), std::move(error_callback)));
}

void RegisterArticleProviderIfEnabled(ContentSuggestionsService* service,
                                      Profile* profile,
                                      UserClassifier* user_classifier,
                                      OfflinePageModel* offline_page_model) {
  if (!IsArticleProviderEnabled()) {
    return;
  }

  PrefService* pref_service = profile->GetPrefs();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  UrlLanguageHistogram* language_histogram =
      UrlLanguageHistogramFactory::GetForBrowserContext(profile);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();

  base::FilePath database_dir(
      profile->GetPath().Append(ntp_snippets::kDatabaseFolder));
  std::string api_key;
  // The API is private. If we don't have the official API key, don't even try.
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    bool is_stable_channel =
        chrome::GetChannel() == version_info::Channel::STABLE;
    api_key = is_stable_channel ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }

  std::unique_ptr<PrefetchedPagesTracker> prefetched_pages_tracker;
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  if (IsKeepingPrefetchedSuggestionsEnabled()) {
    prefetched_pages_tracker =
        std::make_unique<PrefetchedPagesTrackerImpl>(offline_page_model);
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
  auto suggestions_fetcher = std::make_unique<RemoteSuggestionsFetcherImpl>(
      identity_manager, url_loader_factory, pref_service, language_histogram,
      base::BindRepeating(&ParseJson), GetFetchEndpoint(), api_key,
      user_classifier);

  auto provider = std::make_unique<RemoteSuggestionsProviderImpl>(
      service, pref_service, g_browser_process->GetApplicationLocale(),
      service->category_ranker(), service->remote_suggestions_scheduler(),
      std::move(suggestions_fetcher),
      std::make_unique<ImageFetcherImpl>(std::make_unique<ImageDecoderImpl>(),
                                         url_loader_factory),
      std::make_unique<RemoteSuggestionsDatabase>(
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetProtoDatabaseProvider(),
          database_dir),
      std::make_unique<RemoteSuggestionsStatusServiceImpl>(
          identity_manager->HasPrimaryAccount(), pref_service, std::string()),
      std::move(prefetched_pages_tracker),
      std::make_unique<base::OneShotTimer>());

  service->remote_suggestions_scheduler()->SetProvider(provider.get());
  service->set_remote_suggestions_provider(provider.get());
  service->RegisterProvider(std::move(provider));
}

}  // namespace

#endif  // CONTENT_SUGGESTIONS_ENABLED

// static
ContentSuggestionsServiceFactory*
ContentSuggestionsServiceFactory::GetInstance() {
  return base::Singleton<ContentSuggestionsServiceFactory>::get();
}

// static
ContentSuggestionsService* ContentSuggestionsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ContentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContentSuggestionsService*
ContentSuggestionsServiceFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<ContentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

ContentSuggestionsServiceFactory::ContentSuggestionsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ContentSuggestionsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(LargeIconServiceFactory::GetInstance());
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // Depends on OfflinePageModelFactory in SimpleDependencyManager.
  DependsOn(offline_pages::PrefetchServiceFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
}

ContentSuggestionsServiceFactory::~ContentSuggestionsServiceFactory() = default;

KeyedService* ContentSuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(feed::kInterestFeedContentSuggestions)) {
    return nullptr;
  }
#endif  // defined(OS_ANDROID)

#if CONTENT_SUGGESTIONS_ENABLED

  using State = ContentSuggestionsService::State;
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(!profile->IsOffTheRecord());
  PrefService* pref_service = profile->GetPrefs();

  auto user_classifier = std::make_unique<UserClassifier>(
      pref_service, base::DefaultClock::GetInstance());
  auto* user_classifier_raw = user_classifier.get();

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(profile);
#else
  OfflinePageModel* offline_page_model = nullptr;
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  // Create the RemoteSuggestionsScheduler.
  PersistentScheduler* persistent_scheduler = nullptr;
#if defined(OS_ANDROID)
  persistent_scheduler = NTPSnippetsLauncher::Get();
#endif  // OS_ANDROID
  auto scheduler = std::make_unique<RemoteSuggestionsSchedulerImpl>(
      persistent_scheduler, user_classifier_raw, pref_service,
      g_browser_process->local_state(), base::DefaultClock::GetInstance());

  // Create the ContentSuggestionsService.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  favicon::LargeIconService* large_icon_service =
      LargeIconServiceFactory::GetForBrowserContext(profile);
  std::unique_ptr<CategoryRanker> category_ranker =
      ntp_snippets::BuildSelectedCategoryRanker(
          pref_service, base::DefaultClock::GetInstance());

  auto* service = new ContentSuggestionsService(
      State::ENABLED, identity_manager, history_service, large_icon_service,
      pref_service, std::move(category_ranker), std::move(user_classifier),
      std::move(scheduler));

  RegisterArticleProviderIfEnabled(service, profile, user_classifier_raw,
                                   offline_page_model);

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  RegisterWithPrefetching(service, profile);
#endif

  return service;

#else
  return nullptr;
#endif  // CONTENT_SUGGESTIONS_ENABLED
}

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/zero_suggest_cache_service_factory.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/factories/account_password_store_factory.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service_factory.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/strike_database/strike_database_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "extensions/browser/extension_prefs_factory.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/feed/feed_service_factory.h"
#include "components/feed/feed_feature_list.h"
#else
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

// static
ChromeBrowsingDataRemoverDelegateFactory*
ChromeBrowsingDataRemoverDelegateFactory::GetInstance() {
  static base::NoDestructor<ChromeBrowsingDataRemoverDelegateFactory> instance;
  return instance.get();
}

// static
ChromeBrowsingDataRemoverDelegate*
ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(Profile* profile) {
  return static_cast<ChromeBrowsingDataRemoverDelegate*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ChromeBrowsingDataRemoverDelegateFactory::
    ChromeBrowsingDataRemoverDelegateFactory()
    : ProfileKeyedServiceFactory(
          "BrowsingDataRemover",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(AccountPasswordStoreFactory::GetInstance());
  DependsOn(autofill::AutofillEntityDataManagerFactory::GetInstance());
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(DownloadCoreServiceFactory::GetInstance());
  DependsOn(FileSystemAccessPermissionContextFactory::GetInstance());
  DependsOn(FindBarStateFactory::GetInstance());
  DependsOn(HeavyAdServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(MediaDeviceSaltServiceFactory::GetInstance());
  DependsOn(MediaEngagementServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
  DependsOn(payments::BrowserBoundKeyDeleterServiceFactory::GetInstance());
  DependsOn(PermissionActionsHistoryFactory::GetInstance());
  DependsOn(PermissionDecisionAutoBlockerFactory::GetInstance());
  DependsOn(prerender::NoStatePrefetchManagerFactory::GetInstance());
  DependsOn(PrivateVerificationTokensServiceFactory::GetInstance());
  DependsOn(ProtocolHandlerRegistryFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
  DependsOn(safe_browsing::VerdictCacheManagerFactory::GetInstance());
  DependsOn(search_engines::SearchEngineChoiceServiceFactory::GetInstance());
  DependsOn(SearchPrefetchServiceFactory::GetInstance());
  DependsOn(SpellcheckServiceFactory::GetInstance());
  DependsOn(StrikeDatabaseFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(TabRestoreServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(UrlLanguageHistogramFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
  DependsOn(WebHistoryServiceFactory::GetInstance());
  DependsOn(ZeroSuggestCacheServiceFactory::GetInstance());

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  DependsOn(extensions::ActivityLog::GetFactoryInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  DependsOn(SessionServiceFactory::GetInstance());
#endif

#if BUILDFLAG(IS_ANDROID)
  DependsOn(feed::FeedServiceFactory::GetInstance());
  // The delegate also uses offline_pages::OfflinePageModelFactory, but no
  // DependsOn() can be declared for it: it is a SimpleKeyedServiceFactory
  // registered with SimpleDependencyManager, not a
  // BrowserContextKeyedServiceFactory.
#else
  DependsOn(MicrosoftAuthServiceFactory::GetInstance());
#endif  // BUILDFLAG(IS_ANDROID)
}

ChromeBrowsingDataRemoverDelegateFactory::
    ~ChromeBrowsingDataRemoverDelegateFactory() = default;

std::unique_ptr<KeyedService>
ChromeBrowsingDataRemoverDelegateFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // For guest profiles the browsing data is in the OTR profile.
  Profile* profile = static_cast<Profile*>(context);
  CHECK(!profile->IsGuestSession() || profile->IsOffTheRecord(),
        base::NotFatalUntil::M161);
  return std::make_unique<ChromeBrowsingDataRemoverDelegate>(context);
}

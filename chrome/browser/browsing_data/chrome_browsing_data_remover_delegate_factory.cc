// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "extensions/browser/extension_prefs_factory.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if defined(OS_ANDROID)
#include "components/feed/buildflags.h"
#include "components/feed/feed_feature_list.h"
#endif  // defined(OS_ANDROID)

// static
ChromeBrowsingDataRemoverDelegateFactory*
ChromeBrowsingDataRemoverDelegateFactory::GetInstance() {
  return base::Singleton<ChromeBrowsingDataRemoverDelegateFactory>::get();
}

// static
ChromeBrowsingDataRemoverDelegate*
ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(Profile* profile) {
  return static_cast<ChromeBrowsingDataRemoverDelegate*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ChromeBrowsingDataRemoverDelegateFactory::
    ChromeBrowsingDataRemoverDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "BrowsingDataRemover",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(DataReductionProxyChromeSettingsFactory::GetInstance());
#if defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
  if (base::FeatureList::IsEnabled(feed::kInterestFeedContentSuggestions)) {
    DependsOn(feed::FeedHostServiceFactory::GetInstance());
  }
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)
#endif  // defined(OS_ANDROID)
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(prerender::PrerenderManagerFactory::GetInstance());
  DependsOn(TabRestoreServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
  DependsOn(WebHistoryServiceFactory::GetInstance());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(extensions::ActivityLog::GetFactoryInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
#endif

  // In Android, depends on offline_pages::OfflinePageModelFactory in
  // SimpleDependencyManager.

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  DependsOn(SessionServiceFactory::GetInstance());
#endif
}

ChromeBrowsingDataRemoverDelegateFactory::
    ~ChromeBrowsingDataRemoverDelegateFactory() {}

content::BrowserContext*
ChromeBrowsingDataRemoverDelegateFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // For guest profiles the browsing data is in the OTR profile.
  Profile* profile = static_cast<Profile*>(context);
  DCHECK(!profile->IsGuestSession() || profile->IsOffTheRecord());

  return profile;
}

KeyedService* ChromeBrowsingDataRemoverDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChromeBrowsingDataRemoverDelegate(context);
}

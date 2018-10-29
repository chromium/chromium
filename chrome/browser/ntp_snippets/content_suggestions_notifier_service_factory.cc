// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/content_suggestions_notifier_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ntp_snippets/features.h"
#include "components/variations/variations_associated_data.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/ntp/android_content_suggestions_notifier.h"
#include "chrome/browser/android/ntp/content_suggestions_notifier_service.h"
#include "components/feed/feed_feature_list.h"
#endif

using ntp_snippets::kNotificationsFeature;

ContentSuggestionsNotifierServiceFactory*
ContentSuggestionsNotifierServiceFactory::GetInstance() {
  return base::Singleton<ContentSuggestionsNotifierServiceFactory>::get();
}

ContentSuggestionsNotifierService*
ContentSuggestionsNotifierServiceFactory::GetForProfile(Profile* profile) {
#if defined(OS_ANDROID)
  return static_cast<ContentSuggestionsNotifierService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
#else
  return nullptr;
#endif
}

ContentSuggestionsNotifierService*
ContentSuggestionsNotifierServiceFactory::GetForProfileIfExists(
    Profile* profile) {
#if defined(OS_ANDROID)
  return static_cast<ContentSuggestionsNotifierService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
#else
  return nullptr;
#endif
}

ContentSuggestionsNotifierServiceFactory::
    ContentSuggestionsNotifierServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ContentSuggestionsNotifierService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ContentSuggestionsServiceFactory::GetInstance());
}

ContentSuggestionsNotifierServiceFactory::
    ~ContentSuggestionsNotifierServiceFactory() = default;

// BrowserContextKeyedServiceFactory implementation.
KeyedService* ContentSuggestionsNotifierServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(kNotificationsFeature) &&
      !base::FeatureList::IsEnabled(feed::kInterestFeedContentSuggestions)) {
    Profile* profile = Profile::FromBrowserContext(context);
    ntp_snippets::ContentSuggestionsService* suggestions =
        ContentSuggestionsServiceFactory::GetForProfile(profile);
    return new ContentSuggestionsNotifierService(
        profile->GetPrefs(), suggestions,
        std::make_unique<AndroidContentSuggestionsNotifier>());
  } else {
    AndroidContentSuggestionsNotifier().UnregisterChannel();
  }
#endif
  return nullptr;
}

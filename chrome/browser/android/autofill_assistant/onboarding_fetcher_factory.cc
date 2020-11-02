// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/onboarding_fetcher_factory.h"

#include "base/no_destructor.h"
#include "components/autofill_assistant/browser/autofill_assistant_onboarding_fetcher.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

OnboardingFetcherFactory::OnboardingFetcherFactory()
    : BrowserContextKeyedServiceFactory(
          "AutofillAssistantOnboardingFetcher",
          BrowserContextDependencyManager::GetInstance()) {}

OnboardingFetcherFactory::~OnboardingFetcherFactory() = default;

// static
OnboardingFetcherFactory* OnboardingFetcherFactory::GetInstance() {
  static base::NoDestructor<OnboardingFetcherFactory> instance;
  return instance.get();
}

// static
autofill_assistant::AutofillAssistantOnboardingFetcher*
OnboardingFetcherFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<autofill_assistant::AutofillAssistantOnboardingFetcher*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

KeyedService* OnboardingFetcherFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new autofill_assistant::AutofillAssistantOnboardingFetcher(
      content::BrowserContext::GetDefaultStoragePartition(browser_context)
          ->GetURLLoaderFactoryForBrowserProcess());
}

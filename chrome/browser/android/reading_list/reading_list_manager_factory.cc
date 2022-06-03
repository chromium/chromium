// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/reading_list/reading_list_manager_factory.h"

#include <memory>

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/reading_list/android/empty_reading_list_manager.h"
#include "chrome/browser/reading_list/android/reading_list_manager_impl.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/reading_list/features/reading_list_switches.h"

// static
ReadingListManagerFactory* ReadingListManagerFactory::GetInstance() {
  return base::Singleton<ReadingListManagerFactory>::get();
}

// static
ReadingListManager* ReadingListManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReadingListManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

ReadingListManagerFactory::ReadingListManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "ReadingListManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ReadingListModelFactory::GetInstance());
}

ReadingListManagerFactory::~ReadingListManagerFactory() = default;

KeyedService* ReadingListManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(reading_list::switches::kReadLater))
    return new EmptyReadingListManager();

  auto* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(context);
  return new ReadingListManagerImpl(reading_list_model);
}

content::BrowserContext* ReadingListManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

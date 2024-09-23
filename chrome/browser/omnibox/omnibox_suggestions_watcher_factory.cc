// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/omnibox_suggestions_watcher_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/omnibox_suggestions_watcher.h"

// static
OmniboxSuggestionsWatcher*
OmniboxSuggestionsWatcherFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<OmniboxSuggestionsWatcher*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
OmniboxSuggestionsWatcherFactory*
OmniboxSuggestionsWatcherFactory::GetInstance() {
  return base::Singleton<OmniboxSuggestionsWatcherFactory>::get();
}

OmniboxSuggestionsWatcherFactory::OmniboxSuggestionsWatcherFactory()
    : BrowserContextKeyedServiceFactory(
          "OmniboxSuggestionsWatcher",
          BrowserContextDependencyManager::GetInstance()) {}

OmniboxSuggestionsWatcherFactory::~OmniboxSuggestionsWatcherFactory() = default;

KeyedService* OmniboxSuggestionsWatcherFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OmniboxSuggestionsWatcher();
}

content::BrowserContext*
OmniboxSuggestionsWatcherFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

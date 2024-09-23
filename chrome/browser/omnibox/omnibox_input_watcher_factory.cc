// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/omnibox_input_watcher_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/omnibox_input_watcher.h"

// static
OmniboxInputWatcher* OmniboxInputWatcherFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<OmniboxInputWatcher*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
OmniboxInputWatcherFactory* OmniboxInputWatcherFactory::GetInstance() {
  return base::Singleton<OmniboxInputWatcherFactory>::get();
}

OmniboxInputWatcherFactory::OmniboxInputWatcherFactory()
    : BrowserContextKeyedServiceFactory(
          "OmniboxInputWatcher",
          BrowserContextDependencyManager::GetInstance()) {}

OmniboxInputWatcherFactory::~OmniboxInputWatcherFactory() = default;

KeyedService* OmniboxInputWatcherFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OmniboxInputWatcher();
}

content::BrowserContext* OmniboxInputWatcherFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

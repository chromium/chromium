// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMNIBOX_OMNIBOX_INPUT_WATCHER_FACTORY_H_
#define CHROME_BROWSER_OMNIBOX_OMNIBOX_INPUT_WATCHER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class OmniboxInputWatcher;

class OmniboxInputWatcherFactory : public BrowserContextKeyedServiceFactory {
 public:
  static OmniboxInputWatcher* GetForBrowserContext(
      content::BrowserContext* context);

  static OmniboxInputWatcherFactory* GetInstance();

  OmniboxInputWatcherFactory(const OmniboxInputWatcherFactory&) = delete;
  OmniboxInputWatcherFactory& operator=(const OmniboxInputWatcherFactory&) =
      delete;

 private:
  friend base::DefaultSingletonTraits<OmniboxInputWatcherFactory>;

  OmniboxInputWatcherFactory();
  ~OmniboxInputWatcherFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_OMNIBOX_OMNIBOX_INPUT_WATCHER_FACTORY_H_

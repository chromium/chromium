// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CLUSTERS_MEMORIES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_HISTORY_CLUSTERS_MEMORIES_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace memories {
class MemoriesService;
}

// Factory for BrowserContext keyed MemoriesService, which clusters Chrome
// history into useful Memories to be surfaced in UI.
class MemoriesServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static memories::MemoriesService* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  friend base::NoDestructor<MemoriesServiceFactory>;
  static MemoriesServiceFactory& GetInstance();

  MemoriesServiceFactory();
  ~MemoriesServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_HISTORY_CLUSTERS_MEMORIES_SERVICE_FACTORY_H_

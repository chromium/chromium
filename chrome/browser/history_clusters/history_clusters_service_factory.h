// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace history_clusters {
class HistoryClustersService;
}

// Factory for BrowserContext keyed HistoryClustersService, which clusters
// Chrome history into useful Memories to be surfaced in UI.
class HistoryClustersServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // This can return nullptr in tests.
  static history_clusters::HistoryClustersService* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  friend base::NoDestructor<HistoryClustersServiceFactory>;
  static HistoryClustersServiceFactory& GetInstance();

  HistoryClustersServiceFactory();
  ~HistoryClustersServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SERVICE_FACTORY_H_

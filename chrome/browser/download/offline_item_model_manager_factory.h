// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_FACTORY_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class OfflineItemModelManager;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

// This class is the main access point for an OfflineItemModelManager.  It is
// responsible for building the OfflineItemModelManager and associating it with
// a particular content::BrowserContext.
class OfflineItemModelManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns a singleton instance of an OfflineItemModelManagerFactory.
  static OfflineItemModelManagerFactory* GetInstance();

  // Returns the OfflineItemModelManager associated with |context| or creates
  // and associates one if it doesn't exist.
  static OfflineItemModelManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<OfflineItemModelManagerFactory>;

  OfflineItemModelManagerFactory();
  ~OfflineItemModelManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(OfflineItemModelManagerFactory);
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_FACTORY_H_

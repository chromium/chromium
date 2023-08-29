// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_FACTORY_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class OfflineItemModelManager;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

// This class is the main access point for an OfflineItemModelManager.  It is
// responsible for building the OfflineItemModelManager and associating it with
// a particular content::BrowserContext.
class OfflineItemModelManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns a singleton instance of an OfflineItemModelManagerFactory.
  static OfflineItemModelManagerFactory* GetInstance();

  // Returns the OfflineItemModelManager associated with |context| or creates
  // and associates one if it doesn't exist.
  static OfflineItemModelManager* GetForBrowserContext(
      content::BrowserContext* context);

  OfflineItemModelManagerFactory(const OfflineItemModelManagerFactory&) =
      delete;
  OfflineItemModelManagerFactory& operator=(
      const OfflineItemModelManagerFactory&) = delete;

 private:
  friend base::NoDestructor<OfflineItemModelManagerFactory>;

  OfflineItemModelManagerFactory();
  ~OfflineItemModelManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_FACTORY_H_

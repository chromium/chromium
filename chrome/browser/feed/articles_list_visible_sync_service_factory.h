// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_ARTICLES_LIST_VISIBLE_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FEED_ARTICLES_LIST_VISIBLE_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace feed {

// Creates the `ArticlesListVisibleSyncService` of a profile together with the
// profile, so that it observes every change of the articles list visibility.
// A service is only created if `kAndroidFeedsVisibilitySyncByFormFactor` is
// enabled and the device is a phone, a foldable or a tablet.
class ArticlesListVisibleSyncServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  ArticlesListVisibleSyncServiceFactory(
      const ArticlesListVisibleSyncServiceFactory&) = delete;
  ArticlesListVisibleSyncServiceFactory& operator=(
      const ArticlesListVisibleSyncServiceFactory&) = delete;

  static ArticlesListVisibleSyncServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ArticlesListVisibleSyncServiceFactory>;

  ArticlesListVisibleSyncServiceFactory();
  ~ArticlesListVisibleSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_ARTICLES_LIST_VISIBLE_SYNC_SERVICE_FACTORY_H_

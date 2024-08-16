// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_FEED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FEED_FEED_SERVICE_FACTORY_H_

#include <string_view>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace feed {
namespace internal {
const std::string_view GetFollowingFeedFollowCountGroupName(
    size_t follow_count);
}

class FeedService;

// Factory to create one FeedService per browser context. Callers need to
// watch out for nullptr when incognito, as the feed should not be used then.
class FeedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  FeedServiceFactory(const FeedServiceFactory&) = delete;
  FeedServiceFactory& operator=(const FeedServiceFactory&) = delete;

  static FeedService* GetForBrowserContext(content::BrowserContext* context);
  static FeedServiceFactory* GetInstance();
  static std::string GetCountry();

 private:
  friend base::NoDestructor<FeedServiceFactory>;

  FeedServiceFactory();
  ~FeedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_FEED_SERVICE_FACTORY_H_

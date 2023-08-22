// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POWER_BOOKMARKS_POWER_BOOKMARK_SERVICE_FACTORY_H_
#define CHROME_BROWSER_POWER_BOOKMARKS_POWER_BOOKMARK_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace power_bookmarks {
class PowerBookmarkService;
}

// Factory to create one PowerBookmarkService per browser context.
class PowerBookmarkServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static power_bookmarks::PowerBookmarkService* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static PowerBookmarkServiceFactory* GetInstance();

  PowerBookmarkServiceFactory(const PowerBookmarkServiceFactory&) = delete;
  PowerBookmarkServiceFactory& operator=(const PowerBookmarkServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<PowerBookmarkServiceFactory>;

  PowerBookmarkServiceFactory();
  ~PowerBookmarkServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_POWER_BOOKMARKS_POWER_BOOKMARK_SERVICE_FACTORY_H_

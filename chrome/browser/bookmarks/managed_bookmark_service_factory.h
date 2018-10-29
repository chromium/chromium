// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_MANAGED_BOOKMARK_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BOOKMARKS_MANAGED_BOOKMARK_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

class Profile;

namespace bookmarks {
class ManagedBookmarkService;
}

// Singleton that owns all ManagedBookmarkServices and associates them with
// Profile.
class ManagedBookmarkServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static bookmarks::ManagedBookmarkService* GetForProfile(Profile* profile);
  static ManagedBookmarkServiceFactory* GetInstance();
  static TestingFactory GetDefaultFactory();

  static std::string GetManagedBookmarksDomain(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<ManagedBookmarkServiceFactory>;

  ManagedBookmarkServiceFactory();
  ~ManagedBookmarkServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(ManagedBookmarkServiceFactory);
};

#endif  // CHROME_BROWSER_BOOKMARKS_MANAGED_BOOKMARK_SERVICE_FACTORY_H_

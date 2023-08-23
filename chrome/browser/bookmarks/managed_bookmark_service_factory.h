// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_MANAGED_BOOKMARK_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BOOKMARKS_MANAGED_BOOKMARK_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class Profile;

namespace bookmarks {
class ManagedBookmarkService;
}

// Singleton that owns all ManagedBookmarkServices and associates them with
// Profile.
class ManagedBookmarkServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static bookmarks::ManagedBookmarkService* GetForProfile(Profile* profile);
  static ManagedBookmarkServiceFactory* GetInstance();

  ManagedBookmarkServiceFactory(const ManagedBookmarkServiceFactory&) = delete;
  ManagedBookmarkServiceFactory& operator=(
      const ManagedBookmarkServiceFactory&) = delete;

  static TestingFactory GetDefaultFactory();

  static std::string GetManagedBookmarksManager(Profile* profile);

 private:
  friend base::NoDestructor<ManagedBookmarkServiceFactory>;

  ManagedBookmarkServiceFactory();
  ~ManagedBookmarkServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_BOOKMARKS_MANAGED_BOOKMARK_SERVICE_FACTORY_H_

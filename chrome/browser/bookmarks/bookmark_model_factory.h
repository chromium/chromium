// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_FACTORY_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace bookmarks {
class BookmarkModel;
}

// Singleton that builds BookmarkModel instances and associates them with
// BrowserContexts.
class BookmarkModelFactory : public ProfileKeyedServiceFactory {
 public:
  static bookmarks::BookmarkModel* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static bookmarks::BookmarkModel* GetForBrowserContextIfExists(
      content::BrowserContext* browser_context);

  static BookmarkModelFactory* GetInstance();

  BookmarkModelFactory(const BookmarkModelFactory&) = delete;
  BookmarkModelFactory& operator=(const BookmarkModelFactory&) = delete;

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend base::NoDestructor<BookmarkModelFactory>;

  BookmarkModelFactory();
  ~BookmarkModelFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_FACTORY_H_

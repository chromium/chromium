// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class Profile;

class BookmarkMergedSurfaceService;

class BookmarkMergedSurfaceServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static BookmarkMergedSurfaceService* GetForProfile(Profile* profile);
  static BookmarkMergedSurfaceServiceFactory* GetInstance();

  BookmarkMergedSurfaceServiceFactory(
      const BookmarkMergedSurfaceServiceFactory&) = delete;
  BookmarkMergedSurfaceServiceFactory& operator=(
      const BookmarkMergedSurfaceServiceFactory&) = delete;

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend base::NoDestructor<BookmarkMergedSurfaceServiceFactory>;

  BookmarkMergedSurfaceServiceFactory();
  ~BookmarkMergedSurfaceServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_FACTORY_H_

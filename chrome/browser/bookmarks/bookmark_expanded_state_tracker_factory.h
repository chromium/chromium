// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXPANDED_STATE_TRACKER_FACTORY_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXPANDED_STATE_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BookmarkExpandedStateTracker;

class BookmarkExpandedStateTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns nullptr if this profile cannot have a
  // BookmarkExpandedStateTracker.
  static BookmarkExpandedStateTracker* GetForProfile(Profile* profile);

  // Returns an instance of the factory singleton.
  static BookmarkExpandedStateTrackerFactory* GetInstance();

  BookmarkExpandedStateTrackerFactory(
      const BookmarkExpandedStateTrackerFactory&) = delete;
  BookmarkExpandedStateTrackerFactory& operator=(
      const BookmarkExpandedStateTrackerFactory&) = delete;

 private:
  friend class base::NoDestructor<BookmarkExpandedStateTrackerFactory>;

  BookmarkExpandedStateTrackerFactory();
  ~BookmarkExpandedStateTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXPANDED_STATE_TRACKER_FACTORY_H_

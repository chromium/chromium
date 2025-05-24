// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmarks_api_watcher.h"

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {
namespace {

class BookmarksApiWatcherFactory : public ProfileKeyedServiceFactory {
 public:
  static BookmarksApiWatcher* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<BookmarksApiWatcher*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static BookmarksApiWatcherFactory* GetInstance() {
    return base::Singleton<BookmarksApiWatcherFactory>::get();
  }

  BookmarksApiWatcherFactory()
      : ProfileKeyedServiceFactory(
            "BookmarksApiWatcher",
            ProfileSelections::Builder()
                .WithRegular(ProfileSelection::kOwnInstance)
                .WithGuest(ProfileSelection::kOwnInstance)
                // TODO(crbug.com/41488885): Check if this service is needed for
                // Ash Internals.
                .WithAshInternals(ProfileSelection::kOwnInstance)
                .Build()) {}

 private:
  // BrowserContextKeyedServiceFactory overrides
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override {
    return std::make_unique<BookmarksApiWatcher>();
  }
};

}  // namespace

BookmarksApiWatcher::BookmarksApiWatcher() = default;
BookmarksApiWatcher::~BookmarksApiWatcher() = default;

// static
BookmarksApiWatcher* BookmarksApiWatcher::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return BookmarksApiWatcherFactory::GetForBrowserContext(browser_context);
}

void BookmarksApiWatcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BookmarksApiWatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BookmarksApiWatcher::NotifyApiInvoked(const ExtensionFunction* func) {
  for (auto& observer : observers_) {
    observer.OnBookmarksApiInvoked(func);
  }
}

// static
void BookmarksApiWatcher::EnsureFactoryBuilt() {
  BookmarksApiWatcherFactory::GetInstance();
}

}  // namespace extensions

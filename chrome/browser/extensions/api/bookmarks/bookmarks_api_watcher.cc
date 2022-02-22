// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmarks_api_watcher.h"

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {
namespace {

class BookmarksApiWatcherFactory : public BrowserContextKeyedServiceFactory {
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
      : BrowserContextKeyedServiceFactory(
            "BookmarksApiWatcher",
            BrowserContextDependencyManager::GetInstance()) {}

 private:
  // BrowserContextKeyedServiceFactory overrides
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new BookmarksApiWatcher();
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return context;
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

void BookmarksApiWatcher::NotifyApiInvoked(
    const extensions::Extension* extension,
    const extensions::BookmarksFunction* func) {
  for (auto& observer : observers_)
    observer.OnBookmarksApiInvoked(extension, func);
}

}  // namespace extensions

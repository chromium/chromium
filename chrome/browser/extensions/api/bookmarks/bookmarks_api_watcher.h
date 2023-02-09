// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_WATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_WATCHER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"

#include "content/public/browser/browser_context.h"

namespace extensions {
class BookmarksFunction;
class Extension;

// This KeyedService is meant to observe the bookmark api and provide
// notifications
class BookmarksApiWatcher : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Notifies listeners that the bookmark API was invoked.
    virtual void OnBookmarksApiInvoked(
        const extensions::Extension* extension,
        const extensions::BookmarksFunction* func) {}
  };

  static BookmarksApiWatcher* GetForBrowserContext(
      content::BrowserContext* browser_context);

  BookmarksApiWatcher();
  ~BookmarksApiWatcher() override;
  BookmarksApiWatcher(const BookmarksApiWatcher&) = delete;
  BookmarksApiWatcher& operator=(const BookmarksApiWatcher&) = delete;

  void NotifyApiInvoked(const extensions::Extension* extension,
                        const extensions::BookmarksFunction* func);

  // Add/remove observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  static void EnsureFactoryBuilt();

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_WATCHER_H_

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSIONS_ACTIVITY_MONITOR_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSIONS_ACTIVITY_MONITOR_H_

#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api_watcher.h"

class ExtensionFunction;
#endif

namespace syncer {
class ExtensionsActivity;
}

namespace browser_sync {

#if BUILDFLAG(ENABLE_EXTENSIONS)
using BookmarksApiWatcherObserver = extensions::BookmarksApiWatcher::Observer;
#else
// Provides a stub class to inherit from to support overriding the destructor.
class BookmarksApiWatcherObserver {
 public:
  virtual ~BookmarksApiWatcherObserver() {}
};
#endif

// Observe and record usage of extension bookmark API.
class ExtensionsActivityMonitor : public BookmarksApiWatcherObserver {
 public:
  explicit ExtensionsActivityMonitor(content::BrowserContext* context);

  ExtensionsActivityMonitor(const ExtensionsActivityMonitor&) = delete;
  ExtensionsActivityMonitor& operator=(const ExtensionsActivityMonitor&) =
      delete;

  ~ExtensionsActivityMonitor() override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // extensions::BookmarksApiWatcher:
  void OnBookmarksApiInvoked(const ExtensionFunction* func) override;
#endif

  const scoped_refptr<syncer::ExtensionsActivity>& GetExtensionsActivity();

 private:
  scoped_refptr<syncer::ExtensionsActivity> extensions_activity_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  base::ScopedObservation<extensions::BookmarksApiWatcher,
                          extensions::BookmarksApiWatcher::Observer>
      bookmarks_api_observation_{this};
#endif
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSIONS_ACTIVITY_MONITOR_H_

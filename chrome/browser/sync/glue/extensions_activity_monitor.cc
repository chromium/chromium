// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extensions_activity_monitor.h"

#include "components/sync/base/extensions_activity.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api_watcher.h"
#include "extensions/common/extension.h"
#endif

using content::BrowserThread;

namespace browser_sync {

ExtensionsActivityMonitor::ExtensionsActivityMonitor(
    content::BrowserContext* context)
    : extensions_activity_(new syncer::ExtensionsActivity()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // It would be nice if we could specify a Source for each specific function
  // we wanted to observe, but the actual function objects are allocated on
  // the fly so there is no reliable object to point to (same problem if we
  // wanted to use the string name).  Thus, we use all sources and filter in
  // Observe.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  bookmarks_api_observation_.Observe(
      extensions::BookmarksApiWatcher::GetForBrowserContext(context));
#endif
}

ExtensionsActivityMonitor::~ExtensionsActivityMonitor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ExtensionsActivityMonitor::OnBookmarksApiInvoked(
    const extensions::Extension* extension,
    const extensions::BookmarksFunction* func) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!extension) {
    return;
  }

  switch (func->histogram_value()) {
    case extensions::functions::BOOKMARKS_UPDATE:
    case extensions::functions::BOOKMARKS_MOVE:
    case extensions::functions::BOOKMARKS_CREATE:
    case extensions::functions::BOOKMARKS_REMOVETREE:
    case extensions::functions::BOOKMARKS_REMOVE:
      extensions_activity_->UpdateRecord(extension->id());
      break;
    default:
      break;
  }
}
#endif

const scoped_refptr<syncer::ExtensionsActivity>&
ExtensionsActivityMonitor::GetExtensionsActivity() {
  return extensions_activity_;
}

}  // namespace browser_sync

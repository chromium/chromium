// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCHED_PAGES_NOTIFIER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCHED_PAGES_NOTIFIER_H_

#include <string>

#include "components/offline_pages/core/offline_page_types.h"
#include "url/gurl.h"

namespace base {
class Time;
}

namespace offline_pages {

// Shows a notification that informs the user of offline content available at
// |origin|'s host, and that when clicked opens Chrome's download manager.
void ShowPrefetchedContentNotification(const GURL& origin);

// Finds the most recent hostname from a list of pages with the constraint that
// the hostname corresponds to a page created after |pages_created_after|.
// Returns the empty string if no matches are found.
std::u16string ExtractRelevantHostFromOfflinePageItemList(
    const base::Time& pages_created_after,
    const MultipleOfflinePageItemResult page_list);

// Notifies that fresh offline content becomes available. A notification might
// be shown to inform the user.
void OnFreshOfflineContentAvailableForNotification();

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCHED_PAGES_NOTIFIER_H_

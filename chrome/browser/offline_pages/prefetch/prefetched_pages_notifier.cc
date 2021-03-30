// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetched_pages_notifier.h"
#include "components/url_formatter/elide_url.h"

namespace offline_pages {

std::u16string ExtractRelevantHostFromOfflinePageItemList(
    const base::Time& pages_created_after,
    const MultipleOfflinePageItemResult page_list) {
  const OfflinePageItem* newest_page = nullptr;
  base::Time newest_creation_time = pages_created_after;
  for (const OfflinePageItem& page : page_list) {
    // We want to skip pages saved earlier than the most recent match (or the
    // earliest allowable timestamp).
    if (page.creation_time < newest_creation_time)
      continue;

    newest_page = &page;
    newest_creation_time = page.creation_time;
  }

  if (newest_page == nullptr)
    return std::u16string();

  return url_formatter::FormatUrlForSecurityDisplay(
      newest_page->url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
}

}  // namespace offline_pages

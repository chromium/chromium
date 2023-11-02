// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_NAVIGATION_ENTRY_REMOVER_H_
#define CHROME_BROWSER_BROWSING_DATA_NAVIGATION_ENTRY_REMOVER_H_

#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"

class Profile;

namespace browsing_data {

// Remove navigation entries from the tabs of all browsers of |profile|.
// Recent tabs will be cleaned up as well and the session will be rewritten.
// The last session will be removed as it can't be cleaned up easily.
// If a valid |deletion_info.time_range()| is supplied,
// |deletion_info.restrict_urls()| (or all URLs if empty) within this time range
// will be removed and |deletion_info.deleted_rows()| is ignored. Otherwise
// entries matching |deletion_info.deleted_rows()| will be deleted.
void RemoveNavigationEntries(Profile* profile,
                             const history::DeletionInfo& deletion_info);

}  // namespace browsing_data

#endif  // CHROME_BROWSER_BROWSING_DATA_NAVIGATION_ENTRY_REMOVER_H_

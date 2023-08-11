// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_UTIL_H_

#include "chrome/common/extensions/api/reading_list.h"
#include "components/reading_list/core/reading_list_entry.h"

namespace extensions::reading_list_util {

// Converts from ReadingListEntry to api::reading_list::ReadingListEntry.
api::reading_list::ReadingListEntry ParseEntry(const ReadingListEntry& entry);

}  // namespace extensions::reading_list_util

#endif  // CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_UTIL_H_

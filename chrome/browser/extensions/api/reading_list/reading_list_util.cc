// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/reading_list/reading_list_util.h"
#include "base/time/time.h"

namespace extensions::reading_list_util {

api::reading_list::ReadingListEntry ParseEntry(const ReadingListEntry& entry) {
  api::reading_list::ReadingListEntry reading_list_entry;

  reading_list_entry.url = entry.URL().spec();
  reading_list_entry.title = entry.Title();
  reading_list_entry.has_been_read = entry.IsRead();
  reading_list_entry.last_update_time =
      base::Microseconds(entry.UpdateTime()).InMilliseconds();
  reading_list_entry.creation_time =
      base::Microseconds(entry.CreationTime()).InMilliseconds();

  return reading_list_entry;
}

}  // namespace extensions::reading_list_util

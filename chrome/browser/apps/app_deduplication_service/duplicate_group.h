// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_DUPLICATE_GROUP_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_DUPLICATE_GROUP_H_

#include "chrome/browser/apps/app_deduplication_service/entry_types.h"

namespace apps::deduplication {

// Group of duplications. All the entries in this group are considered as
// duplicates.
struct DuplicateGroup {
  DuplicateGroup();

  DuplicateGroup(const DuplicateGroup&) = delete;
  DuplicateGroup& operator=(const DuplicateGroup&) = delete;

  DuplicateGroup(DuplicateGroup&&);
  DuplicateGroup& operator=(DuplicateGroup&&);

  ~DuplicateGroup();

  std::vector<Entry> entries;

  // Other deduplication metadata.
};

}  // namespace apps::deduplication

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_DUPLICATE_GROUP_H_

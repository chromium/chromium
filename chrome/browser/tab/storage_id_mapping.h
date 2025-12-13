// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_ID_MAPPING_H_
#define CHROME_BROWSER_TAB_STORAGE_ID_MAPPING_H_

#include "chrome/browser/tab/storage_id.h"

namespace tabs {

class TabCollection;
class TabInterface;

// Interface for mapping tabs and collections to storage IDs.
class StorageIdMapping {
 public:
  virtual ~StorageIdMapping() = default;

  virtual StorageId GetStorageId(const TabCollection* collection) = 0;
  virtual StorageId GetStorageId(const TabInterface* tab) = 0;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_ID_MAPPING_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STORAGE_TYPE_H_
#define CHROME_BROWSER_TAB_TAB_STORAGE_TYPE_H_

namespace tabs {

// Defines the types of nodes that can be stored in the tab state database.
// These values are persisted to disk. Entries should not be renumbered and
// numeric values should never be reused.
enum class TabStorageType {
  kUnknown = 0,
  kTab = 1,
  kTabStrip = 2,
  kPinned = 3,
  kUnpinned = 4,
  kGroup = 5,
  kSplit = 6,
  kMaxValue = kSplit,
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STORAGE_TYPE_H_

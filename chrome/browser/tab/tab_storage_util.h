// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STORAGE_UTIL_H_
#define CHROME_BROWSER_TAB_TAB_STORAGE_UTIL_H_

#include "chrome/browser/tab/tab_storage_type.h"
#include "components/tabs/public/tab_collection.h"

namespace tabs {

// Converts a TabCollection::Type to a TabStorageType.
TabStorageType TabCollectionTypeToTabStorageType(TabCollection::Type type);

// Converts a TabStorageType to a TabCollection::Type. Returns std::nullopt if
// there is no corresponding collection type.
std::optional<TabCollection::Type> TabStorageTypeToTabCollectionType(
    TabStorageType type);

// Returns true if the given TabStorageType is a collection type.
bool IsTabCollectionStorageType(TabStorageType type);

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STORAGE_UTIL_H_

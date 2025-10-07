// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_util.h"

#include "base/containers/fixed_flat_set.h"
#include "base/notreached.h"
#include "components/tabs/public/tab_collection.h"

namespace tabs {

TabStorageType TabCollectionTypeToTabStorageType(TabCollection::Type type) {
  switch (type) {
    case TabCollection::Type::TABSTRIP:
      return TabStorageType::kTabStrip;
    case TabCollection::Type::PINNED:
      return TabStorageType::kPinned;
    case TabCollection::Type::UNPINNED:
      return TabStorageType::kUnpinned;
    case TabCollection::Type::GROUP:
      return TabStorageType::kGroup;
    case TabCollection::Type::SPLIT:
      return TabStorageType::kSplit;
  }
}

std::optional<TabCollection::Type> TabStorageTypeToTabCollectionType(
    TabStorageType type) {
  switch (type) {
    case TabStorageType::kTabStrip:
      return TabCollection::Type::TABSTRIP;
    case TabStorageType::kPinned:
      return TabCollection::Type::PINNED;
    case TabStorageType::kUnpinned:
      return TabCollection::Type::UNPINNED;
    case TabStorageType::kGroup:
      return TabCollection::Type::GROUP;
    case TabStorageType::kSplit:
      return TabCollection::Type::SPLIT;
    case TabStorageType::kUnknown:
    case TabStorageType::kTab:
      NOTREACHED();
  }
}

bool IsTabCollectionStorageType(TabStorageType type) {
  constexpr auto kCollectionTypes = base::MakeFixedFlatSet<TabStorageType>({
      TabStorageType::kTabStrip,
      TabStorageType::kPinned,
      TabStorageType::kUnpinned,
      TabStorageType::kGroup,
      TabStorageType::kSplit,
  });
  return kCollectionTypes.contains(type);
}

}  // namespace tabs

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

TEST(TabStorageUtilTest, IsTabCollectionStorageType) {
  EXPECT_FALSE(IsTabCollectionStorageType(TabStorageType::kTab));
}

TEST(TabStorageUtilTest, TabCollectionStorageTypeRoundTrip) {
  for (TabCollection::Type type :
       {TabCollection::Type::TABSTRIP, TabCollection::Type::PINNED,
        TabCollection::Type::UNPINNED, TabCollection::Type::GROUP,
        TabCollection::Type::SPLIT}) {
    TabStorageType storage_type = TabCollectionTypeToTabStorageType(type);
    EXPECT_TRUE(IsTabCollectionStorageType(storage_type));
    std::optional<TabCollection::Type> converted_type =
        TabStorageTypeToTabCollectionType(storage_type);
    EXPECT_TRUE(converted_type.has_value());
    EXPECT_EQ(type, converted_type.value());
  }
}

}  // namespace tabs

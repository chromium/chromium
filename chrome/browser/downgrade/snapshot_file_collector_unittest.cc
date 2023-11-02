// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/snapshot_manager.h"

#include "base/containers/flat_set.h"
#include "chrome/browser/downgrade/snapshot_file_collector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace downgrade {

// Tests that ids are not duplicated across the lists of snapshot items.
TEST(SnapshotFileCollector, SnapshotFileIds) {
  auto profile_items = CollectProfileItems();
  auto user_data_items = CollectUserDataItems();
  base::flat_set<SnapshotItemId> ids;
  for (const auto& item : profile_items)
    EXPECT_TRUE(ids.insert(item.id).second) << static_cast<int>(item.id);
  for (const auto& item : user_data_items)
    EXPECT_TRUE(ids.insert(item.id).second) << static_cast<int>(item.id);
}

}  // namespace downgrade

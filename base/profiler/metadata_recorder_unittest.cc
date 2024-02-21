// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/metadata_recorder.h"

#include <optional>

#include "base/ranges/algorithm.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

bool operator==(const MetadataRecorder::Item& lhs,
                const MetadataRecorder::Item& rhs) {
  return lhs.name_hash == rhs.name_hash && lhs.value == rhs.value;
}

bool operator<(const MetadataRecorder::Item& lhs,
               const MetadataRecorder::Item& rhs) {
  return lhs.name_hash < rhs.name_hash;
}

TEST(MetadataRecorderTest, GetItems_Empty) {
  MetadataRecorder recorder;
  MetadataRecorder::ItemArray items;

  EXPECT_EQ(0u, MetadataRecorder::MetadataProvider(&recorder,
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));
  EXPECT_EQ(0u, MetadataRecorder::MetadataProvider(&recorder,
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));
}

TEST(MetadataRecorderTest, Set_NewNameHash) {
  MetadataRecorder recorder;

  recorder.Set(10, std::nullopt, std::nullopt, 20);

  MetadataRecorder::ItemArray items;
  size_t item_count;
  {
    item_count = MetadataRecorder::MetadataProvider(&recorder,
                                                    PlatformThread::CurrentId())
                     .GetItems(&items);
    ASSERT_EQ(1u, item_count);
    EXPECT_EQ(10u, items[0].name_hash);
    EXPECT_FALSE(items[0].key.has_value());
    EXPECT_FALSE(items[0].thread_id.has_value());
    EXPECT_EQ(20, items[0].value);
  }

  recorder.Set(20, std::nullopt, std::nullopt, 30);

  {
    item_count = MetadataRecorder::MetadataProvider(&recorder,
                                                    PlatformThread::CurrentId())
                     .GetItems(&items);
    ASSERT_EQ(2u, item_count);
    EXPECT_EQ(20u, items[1].name_hash);
    EXPECT_FALSE(items[1].key.has_value());
    EXPECT_FALSE(items[1].thread_id.has_value());
    EXPECT_EQ(30, items[1].value);
  }
}

TEST(MetadataRecorderTest, Set_ExistingNameNash) {
  MetadataRecorder recorder;
  recorder.Set(10, std::nullopt, std::nullopt, 20);
  recorder.Set(10, std::nullopt, std::nullopt, 30);

  MetadataRecorder::ItemArray items;
  size_t item_count =
      MetadataRecorder::MetadataProvider(&recorder, PlatformThread::CurrentId())
          .GetItems(&items);
  ASSERT_EQ(1u, item_count);
  EXPECT_EQ(10u, items[0].name_hash);
  EXPECT_FALSE(items[0].key.has_value());
  EXPECT_FALSE(items[0].thread_id.has_value());
  EXPECT_EQ(30, items[0].value);
}

TEST(MetadataRecorderTest, Set_ReAddRemovedNameNash) {
  MetadataRecorder recorder;
  MetadataRecorder::ItemArray items;
  std::vector<MetadataRecorder::Item> expected;
  for (size_t i = 0; i < items.size(); ++i) {
    expected.emplace_back(i, std::nullopt, std::nullopt, 0);
    recorder.Set(i, std::nullopt, std::nullopt, 0);
  }

  // By removing an item from a full recorder, re-setting the same item, and
  // verifying that the item is returned, we can verify that the recorder is
  // reusing the inactive slot for the same name hash instead of trying (and
  // failing) to allocate a new slot.
  recorder.Remove(3, std::nullopt, std::nullopt);
  recorder.Set(3, std::nullopt, std::nullopt, 0);

  size_t item_count =
      MetadataRecorder::MetadataProvider(&recorder, PlatformThread::CurrentId())
          .GetItems(&items);
  EXPECT_EQ(items.size(), item_count);
  EXPECT_THAT(expected, ::testing::UnorderedElementsAreArray(items));
}

TEST(MetadataRecorderTest, Set_AddPastMaxCount) {
  MetadataRecorder recorder;
  MetadataRecorder::ItemArray items;
  for (size_t i = 0; i < items.size(); ++i) {
    recorder.Set(i, std::nullopt, std::nullopt, 0);
  }

  // This should fail silently.
  recorder.Set(items.size(), std::nullopt, std::nullopt, 0);
}

TEST(MetadataRecorderTest, Set_NulloptKeyIsIndependentOfNonNulloptKey) {
  MetadataRecorder recorder;

  recorder.Set(10, 100, std::nullopt, 20);

  MetadataRecorder::ItemArray items;
  size_t item_count;
  {
    item_count = MetadataRecorder::MetadataProvider(&recorder,
                                                    PlatformThread::CurrentId())
                     .GetItems(&items);
    ASSERT_EQ(1u, item_count);
    EXPECT_EQ(10u, items[0].name_hash);
    ASSERT_TRUE(items[0].key.has_value());
    EXPECT_EQ(100, *items[0].key);
    EXPECT_EQ(20, items[0].value);
  }

  recorder.Set(10, std::nullopt, std::nullopt, 30);

  {
    item_count = MetadataRecorder::MetadataProvider(&recorder,
                                                    PlatformThread::CurrentId())
                     .GetItems(&items);
    ASSERT_EQ(2u, item_count);

    EXPECT_EQ(10u, items[0].name_hash);
    ASSERT_TRUE(items[0].key.has_value());
    EXPECT_EQ(100, *items[0].key);
    EXPECT_EQ(20, items[0].value);

    EXPECT_EQ(10u, items[1].name_hash);
    EXPECT_FALSE(items[1].key.has_value());
    EXPECT_EQ(30, items[1].value);
  }
}

TEST(MetadataRecorderTest, Set_ThreadIdIsScoped) {
  MetadataRecorder recorder;

  recorder.Set(10, std::nullopt, PlatformThread::CurrentId(), 20);

  MetadataRecorder::ItemArray items;
  size_t item_count;
  {
    item_count = MetadataRecorder::MetadataProvider(&recorder,
                                                    PlatformThread::CurrentId())
                     .GetItems(&items);
    ASSERT_EQ(1u, item_count);
    EXPECT_EQ(10u, items[0].name_hash);
    EXPECT_FALSE(items[0].key.has_value());
    ASSERT_TRUE(items[0].thread_id.has_value());
    EXPECT_EQ(PlatformThread::CurrentId(), *items[0].thread_id);
    EXPECT_EQ(20, items[0].value);
  }
  {
    item_count = MetadataRecorder::MetadataProvider(&recorder, kInvalidThreadId)
                     .GetItems(&items);
    EXPECT_EQ(0U, item_count);
  }
}

TEST(MetadataRecorderTest, Set_NulloptThreadAndNonNulloptThread) {
  MetadataRecorder recorder;

  recorder.Set(10, std::nullopt, PlatformThread::CurrentId(), 20);
  recorder.Set(10, std::nullopt, std::nullopt, 30);

  MetadataRecorder::ItemArray items;
  size_t item_count =
      MetadataRecorder::MetadataProvider(&recorder, PlatformThread::CurrentId())
          .GetItems(&items);
  ASSERT_EQ(2u, item_count);
  EXPECT_EQ(10u, items[0].name_hash);
  EXPECT_FALSE(items[0].key.has_value());
  ASSERT_TRUE(items[0].thread_id.has_value());
  EXPECT_EQ(PlatformThread::CurrentId(), *items[0].thread_id);
  EXPECT_EQ(20, items[0].value);

  EXPECT_EQ(10u, items[1].name_hash);
  EXPECT_FALSE(items[1].key.has_value());
  EXPECT_FALSE(items[1].thread_id.has_value());
  EXPECT_EQ(30, items[1].value);
}

TEST(MetadataRecorderTest, Remove) {
  MetadataRecorder recorder;
  recorder.Set(10, std::nullopt, std::nullopt, 20);
  recorder.Set(30, std::nullopt, std::nullopt, 40);
  recorder.Set(50, std::nullopt, std::nullopt, 60);
  recorder.Remove(30, std::nullopt, std::nullopt);

  MetadataRecorder::ItemArray items;
  size_t item_count =
      MetadataRecorder::MetadataProvider(&recorder, PlatformThread::CurrentId())
          .GetItems(&items);
  ASSERT_EQ(2u, item_count);
  EXPECT_EQ(10u, items[0].name_hash);
  EXPECT_FALSE(items[0].key.has_value());
  EXPECT_EQ(20, items[0].value);
  EXPECT_EQ(50u, items[1].name_hash);
  EXPECT_FALSE(items[1].key.has_value());
  EXPECT_EQ(60, items[1].value);
}

TEST(MetadataRecorderTest, Remove_DoesntExist) {
  MetadataRecorder recorder;
  recorder.Set(10, std::nullopt, std::nullopt, 20);
  recorder.Remove(20, std::nullopt, std::nullopt);

  MetadataRecorder::ItemArray items;
  size_t item_count =
      MetadataRecorder::MetadataProvider(&recorder, PlatformThread::CurrentId())
          .GetItems(&items);
  ASSERT_EQ(1u, item_count);
  EXPECT_EQ(10u, items[0].name_hash);
  EXPECT_FALSE(items[0].key.has_value());
  EXPECT_EQ(20, items[0].value);
}

TEST(MetadataRecorderTest, Remove_NulloptKeyIsIndependentOfNonNulloptKey) {
  MetadataRecorder recorder;

  recorder.Set(10, 100, std::nullopt, 20);
  recorder.Set(10, std::nullopt, std::nullopt, 30);

  recorder.Remove(10, std::nullopt, std::nullopt);

  MetadataRecorder::ItemArray items;
  size_t item_count =
      MetadataRecorder::MetadataProvider(&recorder, PlatformThread::CurrentId())
          .GetItems(&items);
  ASSERT_EQ(1u, item_count);
  EXPECT_EQ(10u, items[0].name_hash);
  ASSERT_TRUE(items[0].key.has_value());
  EXPECT_EQ(100, *items[0].key);
  EXPECT_EQ(20, items[0].value);
}

TEST(MetadataRecorderTest,
     Remove_NulloptThreadIsIndependentOfNonNulloptThread) {
  MetadataRecorder recorder;

  recorder.Set(10, std::nullopt, PlatformThread::CurrentId(), 20);
  recorder.Set(10, std::nullopt, std::nullopt, 30);

  recorder.Remove(10, std::nullopt, std::nullopt);

  MetadataRecorder::ItemArray items;
  size_t item_count =
      MetadataRecorder::MetadataProvider(&recorder, PlatformThread::CurrentId())
          .GetItems(&items);
  ASSERT_EQ(1u, item_count);
  EXPECT_EQ(10u, items[0].name_hash);
  EXPECT_FALSE(items[0].key.has_value());
  ASSERT_TRUE(items[0].thread_id.has_value());
  EXPECT_EQ(PlatformThread::CurrentId(), *items[0].thread_id);
  EXPECT_EQ(20, items[0].value);
}

TEST(MetadataRecorderTest, ReclaimInactiveSlots) {
  MetadataRecorder recorder;

  std::set<MetadataRecorder::Item> items_set;
  // Fill up the metadata map.
  for (size_t i = 0; i < MetadataRecorder::MAX_METADATA_COUNT; ++i) {
    recorder.Set(i, std::nullopt, std::nullopt, i);
    items_set.insert(MetadataRecorder::Item{i, std::nullopt, std::nullopt,
                                            static_cast<int64_t>(i)});
  }

  // Remove every fourth entry to fragment the data.
  size_t entries_removed = 0;
  for (size_t i = 3; i < MetadataRecorder::MAX_METADATA_COUNT; i += 4) {
    recorder.Remove(i, std::nullopt, std::nullopt);
    ++entries_removed;
    items_set.erase(MetadataRecorder::Item{i, std::nullopt, std::nullopt,
                                           static_cast<int64_t>(i)});
  }

  // Ensure that the inactive slots are reclaimed to make room for more entries.
  for (size_t i = 1; i <= entries_removed; ++i) {
    recorder.Set(i * 100, std::nullopt, std::nullopt, i * 100);
    items_set.insert(MetadataRecorder::Item{i * 100, std::nullopt, std::nullopt,
                                            static_cast<int64_t>(i * 100)});
  }

  MetadataRecorder::ItemArray items_arr;
  ranges::copy(items_set, items_arr.begin());

  MetadataRecorder::ItemArray recorder_items;
  size_t recorder_item_count =
      MetadataRecorder::MetadataProvider(&recorder, PlatformThread::CurrentId())
          .GetItems(&recorder_items);
  EXPECT_EQ(recorder_item_count, MetadataRecorder::MAX_METADATA_COUNT);
  EXPECT_THAT(recorder_items, ::testing::UnorderedElementsAreArray(items_arr));
}

}  // namespace base

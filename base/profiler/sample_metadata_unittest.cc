// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/sample_metadata.h"

#include "base/metrics/metrics_hashes.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(SampleMetadataTest, ScopedSampleMetadata) {
  MetadataRecorder::ItemArray items;
  // TODO(https://crbug/1494111): Locate the other tests that are leaving items
  // in MetadataRecorder and update them to clean up the state.
  size_t initial_item_count =
      MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                         PlatformThread::CurrentId())
          .GetItems(&items);

  {
    ScopedSampleMetadata m("myname", 100, SampleMetadataScope::kProcess);

    ASSERT_EQ(initial_item_count + 1,
              MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                 PlatformThread::CurrentId())
                  .GetItems(&items));
    EXPECT_EQ(HashMetricName("myname"), items[initial_item_count].name_hash);
    EXPECT_FALSE(items[initial_item_count].key.has_value());
    EXPECT_EQ(100, items[initial_item_count].value);
  }

  ASSERT_EQ(initial_item_count,
            MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                               PlatformThread::CurrentId())
                .GetItems(&items));
}

TEST(SampleMetadataTest, ScopedSampleMetadataWithKey) {
  MetadataRecorder::ItemArray items;
  // TODO(https://crbug/1494111): Locate the other tests that are leaving items
  // in MetadataRecorder and update them to clean up the state.
  size_t initial_item_count =
      MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                         PlatformThread::CurrentId())
          .GetItems(&items);

  {
    ScopedSampleMetadata m("myname", 10, 100, SampleMetadataScope::kProcess);

    ASSERT_EQ(initial_item_count + 1,
              MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                 PlatformThread::CurrentId())
                  .GetItems(&items));
    EXPECT_EQ(HashMetricName("myname"), items[initial_item_count].name_hash);
    ASSERT_TRUE(items[initial_item_count].key.has_value());
    EXPECT_EQ(10, *items[initial_item_count].key);
    EXPECT_EQ(100, items[initial_item_count].value);
  }

  ASSERT_EQ(initial_item_count,
            MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                               PlatformThread::CurrentId())
                .GetItems(&items));
}

TEST(SampleMetadataTest, SampleMetadata) {
  MetadataRecorder::ItemArray items;
  // TODO(https://crbug/1494111): Locate the other tests that are leaving items
  // in MetadataRecorder and update them to clean up the state.
  size_t initial_item_count =
      MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                         PlatformThread::CurrentId())
          .GetItems(&items);

  SampleMetadata metadata("myname", SampleMetadataScope::kProcess);
  metadata.Set(100);
  ASSERT_EQ(initial_item_count + 1,
            MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                               PlatformThread::CurrentId())
                .GetItems(&items));
  EXPECT_EQ(HashMetricName("myname"), items[initial_item_count].name_hash);
  EXPECT_FALSE(items[initial_item_count].key.has_value());
  EXPECT_EQ(100, items[initial_item_count].value);

  metadata.Remove();
  ASSERT_EQ(initial_item_count,
            MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                               PlatformThread::CurrentId())
                .GetItems(&items));
}

TEST(SampleMetadataTest, SampleMetadataWithKey) {
  MetadataRecorder::ItemArray items;
  // TODO(https://crbug/1494111): Locate the other tests that are leaving items
  // in MetadataRecorder and update them to clean up the state.
  size_t initial_item_count =
      MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                         PlatformThread::CurrentId())
          .GetItems(&items);

  SampleMetadata metadata("myname", SampleMetadataScope::kProcess);
  metadata.Set(10, 100);
  ASSERT_EQ(initial_item_count + 1,
            MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                               PlatformThread::CurrentId())
                .GetItems(&items));
  EXPECT_EQ(HashMetricName("myname"), items[initial_item_count].name_hash);
  ASSERT_TRUE(items[initial_item_count].key.has_value());
  EXPECT_EQ(10, *items[initial_item_count].key);
  EXPECT_EQ(100, items[initial_item_count].value);

  metadata.Remove(10);
  ASSERT_EQ(initial_item_count,
            MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                               PlatformThread::CurrentId())
                .GetItems(&items));
}

TEST(SampleMetadataTest, SampleMetadataWithThreadId) {
  MetadataRecorder::ItemArray items;
  // TODO(https://crbug/1494111): Locate the other tests that are leaving items
  // in MetadataRecorder and update them to clean up the state.
  size_t initial_item_count =
      MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                         PlatformThread::CurrentId())
          .GetItems(&items);

  SampleMetadata metadata("myname", SampleMetadataScope::kThread);
  metadata.Set(100);
  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   kInvalidThreadId)
                    .GetItems(&items));
  ASSERT_EQ(initial_item_count + 1,
            MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                               PlatformThread::CurrentId())
                .GetItems(&items));
  EXPECT_EQ(HashMetricName("myname"), items[initial_item_count].name_hash);
  EXPECT_FALSE(items[initial_item_count].key.has_value());
  EXPECT_EQ(100, items[initial_item_count].value);

  metadata.Remove();
  ASSERT_EQ(initial_item_count,
            MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                               PlatformThread::CurrentId())
                .GetItems(&items));
}

}  // namespace base

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
  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));

  {
    ScopedSampleMetadata m("myname", 100, SampleMetadataScope::kProcess);

    ASSERT_EQ(1u, MetadataRecorder::MetadataProvider(
                      GetSampleMetadataRecorder(), PlatformThread::CurrentId())
                      .GetItems(&items));
    EXPECT_EQ(HashMetricName("myname"), items[0].name_hash);
    EXPECT_FALSE(items[0].key.has_value());
    EXPECT_EQ(100, items[0].value);
  }

  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));
}

TEST(SampleMetadataTest, ScopedSampleMetadataWithKey) {
  MetadataRecorder::ItemArray items;
  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));

  {
    ScopedSampleMetadata m("myname", 10, 100, SampleMetadataScope::kProcess);

    ASSERT_EQ(1u, MetadataRecorder::MetadataProvider(
                      GetSampleMetadataRecorder(), PlatformThread::CurrentId())
                      .GetItems(&items));
    EXPECT_EQ(HashMetricName("myname"), items[0].name_hash);
    ASSERT_TRUE(items[0].key.has_value());
    EXPECT_EQ(10, *items[0].key);
    EXPECT_EQ(100, items[0].value);
  }

  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));
}

// Test is flaky on iOS. crbug.com/1494111
#if BUILDFLAG(IS_IOS)
#define MAYBE_SampleMetadata DISABLED_SampleMetadata
#else
#define MAYBE_SampleMetadata SampleMetadata
#endif
TEST(SampleMetadataTest, MAYBE_SampleMetadata) {
  MetadataRecorder::ItemArray items;
  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));

  SampleMetadata metadata("myname", SampleMetadataScope::kProcess);
  metadata.Set(100);
  ASSERT_EQ(1u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));
  EXPECT_EQ(HashMetricName("myname"), items[0].name_hash);
  EXPECT_FALSE(items[0].key.has_value());
  EXPECT_EQ(100, items[0].value);

  metadata.Remove();
  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));
}

// Test is flaky on iOS. crbug.com/1494111
#if BUILDFLAG(IS_IOS)
#define MAYBE_SampleMetadataWithKey DISABLED_SampleMetadataWithKey
#else
#define MAYBE_SampleMetadataWithKey SampleMetadataWithKey
#endif
TEST(SampleMetadataTest, MAYBE_SampleMetadataWithKey) {
  MetadataRecorder::ItemArray items;
  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));

  SampleMetadata metadata("myname", SampleMetadataScope::kProcess);
  metadata.Set(10, 100);
  ASSERT_EQ(1u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));
  EXPECT_EQ(HashMetricName("myname"), items[0].name_hash);
  ASSERT_TRUE(items[0].key.has_value());
  EXPECT_EQ(10, *items[0].key);
  EXPECT_EQ(100, items[0].value);

  metadata.Remove(10);
  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));
}

// Test is flaky on iOS. crbug.com/1494111
#if BUILDFLAG(IS_IOS)
#define MAYBE_SampleMetadataWithThreadId DISABLED_SampleMetadataWithThreadId
#else
#define MAYBE_SampleMetadataWithThreadId SampleMetadataWithThreadId
#endif
TEST(SampleMetadataTest, MAYBE_SampleMetadataWithThreadId) {
  MetadataRecorder::ItemArray items;
  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));

  SampleMetadata metadata("myname", SampleMetadataScope::kThread);
  metadata.Set(100);
  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   kInvalidThreadId)
                    .GetItems(&items));
  ASSERT_EQ(1u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));
  EXPECT_EQ(HashMetricName("myname"), items[0].name_hash);
  EXPECT_FALSE(items[0].key.has_value());
  EXPECT_EQ(100, items[0].value);

  metadata.Remove();
  ASSERT_EQ(0u, MetadataRecorder::MetadataProvider(GetSampleMetadataRecorder(),
                                                   PlatformThread::CurrentId())
                    .GetItems(&items));
}

}  // namespace base

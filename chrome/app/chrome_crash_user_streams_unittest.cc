// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_crash_user_streams.h"

#include <stdint.h>

#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "components/crash/core/common/shared_memory_user_stream_reader.h"
#include "components/metrics/system_profile_user_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(ChromeCrashUserStreamsTest, GetSharedMemoryRegionsReturnsSameRegion) {
  ChromeCrashUserStreams streams;

  std::vector<base::ReadOnlySharedMemoryRegion> regions1 =
      streams.GetSharedMemoryRegions();
  ASSERT_EQ(regions1.size(), 1u);
  EXPECT_TRUE(regions1[0].IsValid());

  std::vector<base::ReadOnlySharedMemoryRegion> regions2 =
      streams.GetSharedMemoryRegions();
  ASSERT_EQ(regions2.size(), 1u);
  EXPECT_TRUE(regions2[0].IsValid());

  EXPECT_EQ(regions1[0].GetGUID(), regions2[0].GetGUID());
}

TEST(ChromeCrashUserStreamsTest, WriteStoresSystemProfilePayload) {
  ChromeCrashUserStreams streams;
  std::vector<base::ReadOnlySharedMemoryRegion> regions =
      streams.GetSharedMemoryRegions();
  ASSERT_EQ(regions.size(), 1u);
  ASSERT_TRUE(regions[0].IsValid());

  const std::vector<uint8_t> payload = {1, 2, 3, 4};
  EXPECT_TRUE(streams.Write(metrics::kSystemProfileUserStreamType, payload));

  base::ReadOnlySharedMemoryMapping mapping = regions[0].Map();
  ASSERT_TRUE(mapping.IsValid());

  ASSERT_OK_AND_ASSIGN(crash_reporter::UserStreamData data,
                       crash_reporter::ExtractSharedMemoryUserStreamData(
                           mapping.GetMemoryAsSpan<const uint8_t>()));
  EXPECT_EQ(data.user_stream_type, metrics::kSystemProfileUserStreamType);
  EXPECT_EQ(data.payload, payload);
}

TEST(ChromeCrashUserStreamsTest, WriteRejectsUnknownStreamType) {
  ChromeCrashUserStreams streams;
  ASSERT_EQ(streams.GetSharedMemoryRegions().size(), 1u);

  constexpr uint32_t kUnknownStreamType = 0x4B6B9999;
  const std::vector<uint8_t> payload = {1, 2, 3, 4};
  EXPECT_FALSE(streams.Write(kUnknownStreamType, payload));
}

TEST(ChromeCrashUserStreamsTest, ExactCapacityPayloadIsWritten) {
  ChromeCrashUserStreams streams;
  ASSERT_EQ(streams.GetSharedMemoryRegions().size(), 1u);

  const std::vector<uint8_t> payload(
      metrics::kSystemProfileUserStreamCapacityBytes, 'Z');
  EXPECT_TRUE(streams.Write(metrics::kSystemProfileUserStreamType, payload));
}

TEST(ChromeCrashUserStreamsDeathTest, OversizedPayloadChecks) {
  ChromeCrashUserStreams streams;
  ASSERT_EQ(streams.GetSharedMemoryRegions().size(), 1u);

  const std::vector<uint8_t> oversized_payload(
      metrics::kSystemProfileUserStreamCapacityBytes + 1, 'Z');
  EXPECT_CHECK_DEATH(
      streams.Write(metrics::kSystemProfileUserStreamType, oversized_payload));
}

}  // namespace

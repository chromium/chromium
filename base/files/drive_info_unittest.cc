// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/drive_info.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_file.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using ::testing::Optional;

void TestForReasonableDriveInfo(const std::optional<DriveInfo>& info) {
  ASSERT_TRUE(info.has_value());

  // `has_seek_penalty` may or may not be true but should be ascertainable.
  EXPECT_TRUE(info->has_seek_penalty.has_value());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  // `is_removable` may or may not be true but should be ascertainable.
  EXPECT_TRUE(info->is_removable.has_value());

  // Expect more than 10MB for the media size.
  EXPECT_THAT(info->size_bytes, Optional(testing::Ge(10'000'000)));
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // `is_usb` may or may not be true but should be ascertainable.
  EXPECT_TRUE(info->is_usb.has_value());
#endif

#if BUILDFLAG(IS_MAC)
  // Nothing should be CoreStorage any more on the Mac.
  EXPECT_THAT(info->is_core_storage, Optional(false));

  // Everything should be APFS nowadays.
  EXPECT_THAT(info->is_apfs, Optional(true));

  // This test should not encounter a read-only drive.
  EXPECT_THAT(info->is_writable, Optional(true));

  EXPECT_THAT(info->bsd_name, Optional(testing::StartsWith("disk")));
#endif
}

}  // namespace

// A test for the main entry point, GetFileDriveInfo(). Note that on the Mac,
// the code goes:
//
// GetFileDriveInfo() -> GetIOObjectDriveInfo()
//
// so this single test does test all entrypoints.
TEST(DriveInfoTest, GetFileDriveInfo) {
  ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.Create());
  std::optional<DriveInfo> info = GetFileDriveInfo(temp_file.path());

  TestForReasonableDriveInfo(info);
}

}  // namespace base

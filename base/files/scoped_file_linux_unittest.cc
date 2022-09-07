// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_file.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class ScopedFDOwnershipTrackingTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }
  void TearDown() override { ASSERT_TRUE(temp_dir_.Delete()); }

  ScopedFD OpenFD() {
    FilePath dont_care;
    return CreateAndOpenFdForTemporaryFileInDir(temp_dir_.GetPath(),
                                                &dont_care);
  }

 private:
  ScopedTempDir temp_dir_;
};

TEST_F(ScopedFDOwnershipTrackingTest, BasicTracking) {
  ScopedFD fd = OpenFD();
  EXPECT_TRUE(IsFDOwned(fd.get()));
  int fd_value = fd.get();
  fd.reset();
  EXPECT_FALSE(IsFDOwned(fd_value));
}

#if defined(GTEST_HAS_DEATH_TEST)

TEST_F(ScopedFDOwnershipTrackingTest, NoDoubleOwnership) {
  ScopedFD fd = OpenFD();
  subtle::EnableFDOwnershipEnforcement(true);
  EXPECT_DEATH(ScopedFD(fd.get()), "");
}

TEST_F(ScopedFDOwnershipTrackingTest, CrashOnUnownedClose) {
  ScopedFD fd = OpenFD();
  subtle::EnableFDOwnershipEnforcement(true);
  EXPECT_DEATH(close(fd.get()), "");
}

#endif  // defined(GTEST_HAS_DEATH_TEST)

}  // namespace
}  // namespace base

// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_util.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_NACL)
#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE

namespace base {
namespace i18n {

class IcuUtilTest : public testing::Test {
 protected:
  void SetUp() override { ResetGlobalsForTesting(); }
};

#if defined(OS_ANDROID)

TEST_F(IcuUtilTest, InitializeIcuSucceeds) {
  bool success = InitializeICU();

  ASSERT_TRUE(success);
}

TEST_F(IcuUtilTest, ExtraFileNotInitializedAtStart) {
  MemoryMappedFile::Region region;
  PlatformFile file = GetIcuExtraDataFileHandle(&region);

  ASSERT_EQ(file, kInvalidPlatformFile);
}

TEST_F(IcuUtilTest, InitializeExtraIcuSucceeds) {
  bool success = InitializeExtraICU(std::string());

  ASSERT_TRUE(success);
}

TEST_F(IcuUtilTest, CannotInitializeExtraIcuAfterIcu) {
  InitializeICU();
  bool success = InitializeExtraICU(std::string());

  ASSERT_FALSE(success);
}

TEST_F(IcuUtilTest, ExtraFileInitializedAfterInit) {
  InitializeExtraICU(std::string());
  MemoryMappedFile::Region region;
  PlatformFile file = GetIcuExtraDataFileHandle(&region);

  ASSERT_NE(file, kInvalidPlatformFile);
}

TEST_F(IcuUtilTest, InitializeExtraIcuFromFdSucceeds) {
  InitializeExtraICU(std::string());
  MemoryMappedFile::Region region;
  PlatformFile pf = GetIcuExtraDataFileHandle(&region);
  bool success = InitializeExtraICUWithFileDescriptor(pf, region);

  ASSERT_TRUE(success);
}

TEST_F(IcuUtilTest, CannotInitializeExtraIcuFromFdAfterIcu) {
  InitializeExtraICU(std::string());
  InitializeICU();
  MemoryMappedFile::Region region;
  PlatformFile pf = GetIcuExtraDataFileHandle(&region);
  bool success = InitializeExtraICUWithFileDescriptor(pf, region);

  ASSERT_FALSE(success);
}

#endif  // defined(OS_ANDROID)

}  // namespace i18n
}  // namespace base

#endif  // ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
#endif  // !defined(OS_NACL)

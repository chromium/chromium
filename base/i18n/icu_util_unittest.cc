// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_util.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_NACL)
#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE

namespace base::i18n {

class IcuUtilTest : public testing::Test {
 protected:
  void SetUp() override { ResetGlobalsForTesting(); }
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(IcuUtilTest, InitializeIcuSucceeds) {
  bool success = InitializeICU();

  ASSERT_TRUE(success);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_ANDROID)

TEST_F(IcuUtilTest, InitializeIcuSucceeds) {
  bool success = InitializeICU();

  ASSERT_TRUE(success);
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace base::i18n

#endif  // ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
#endif  // !BUILDFLAG(IS_NACL)

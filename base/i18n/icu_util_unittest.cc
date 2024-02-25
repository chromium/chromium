// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_util.h"

#include "base/test/icu_test_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_NACL) && (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE) && \
    (BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_ANDROID))

namespace base::i18n {

class IcuUtilTest : public testing::Test {
 protected:
  void SetUp() override { ResetGlobalsForTesting(); }
  void TearDown() override {
    ResetGlobalsForTesting();

    // ICU must be set back up in case e.g. a log statement that formats times
    // uses it.
    test::InitializeICUForTesting();
  }
};

TEST_F(IcuUtilTest, InitializeIcuSucceeds) {
  bool success = InitializeICU();

  ASSERT_TRUE(success);
}

}  // namespace base::i18n

#endif

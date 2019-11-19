// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/consistency_golden_loader.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/proto_matcher.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace utils = time_limit_test_utils;

namespace time_limit_consistency {

using ConsistencyGoldenLoaderTest = testing::Test;

base::FilePath GetTestGoldensPath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);

  return path.Append(
      FILE_PATH_LITERAL("chrome/browser/chromeos/child_accounts/"
                        "time_limit_consistency_test/test_goldens"));
}

// Expected outcome is to ignore the test_golden_unsupported suite and return
// only the case within test_golden.
TEST_F(ConsistencyGoldenLoaderTest, LoadTestGoldenCases) {
  std::vector<GoldenParam> goldens_list =
      LoadGoldenCasesFromPath(GetTestGoldensPath());

  ConsistencyGoldenCase golden_case;
  golden_case.mutable_current_state()->set_time_millis(42);

  ASSERT_EQ(goldens_list.size(), 1ul);
  EXPECT_EQ(goldens_list[0].suite_name, "test_golden");
  EXPECT_EQ(goldens_list[0].index, 0);
  EXPECT_THAT(goldens_list[0].golden_case, EqualsProto(golden_case));
}

}  // namespace time_limit_consistency
}  // namespace chromeos

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test for the time limit processor which checks its behavior against golden
// files. This is a parameterized test, so the single test method will actually
// produce one test case for each case listed inside each golden file.

#include <memory.h>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/consistency_golden_converter.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/consistency_golden_loader.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/goldens/consistency_golden.pb.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/proto_matcher.h"
#include "chrome/browser/chromeos/child_accounts/usage_time_limit_processor.h"
#include "chromeos/settings/timezone_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace time_limit_consistency {

using TimeLimitConsistencyTest = testing::TestWithParam<GoldenParam>;

// The main test case for the consistency tests, which will be run once for each
// golden case.
TEST_P(TimeLimitConsistencyTest, OutputMatchesGolden) {
  const ConsistencyGoldenCase golden_case = GetParam().golden_case;
  const ConsistencyGoldenCurrentState current_state =
      golden_case.current_state();

  std::unique_ptr<icu::TimeZone> timezone(
      icu::TimeZone::createTimeZone(current_state.timezone().c_str()));
  base::Time current_time =
      base::Time::FromJavaTime(current_state.time_millis());
  base::Time usage_timestamp =
      base::Time::FromJavaTime(current_state.usage_timestamp());
  base::Optional<usage_time_limit::State> previous_state =
      GenerateUnlockUsageLimitOverrideStateFromInput(golden_case.input());

  base::Value policy = ConvertGoldenInputToProcessorInput(golden_case.input());
  usage_time_limit::State state = usage_time_limit::GetState(
      policy, /* local_override */ nullptr,
      base::TimeDelta::FromMilliseconds(current_state.usage_millis()),
      usage_timestamp, current_time, timezone.get(), previous_state);
  ConsistencyGoldenOutput actual_output =
      ConvertProcessorOutputToGoldenOutput(state);

  EXPECT_THAT(actual_output, EqualsProto(golden_case.output()));
}

// Generate the test case name from the metadata included in GoldenParam.
static std::string GetTestCaseName(
    testing::TestParamInfo<GoldenParam> param_info) {
  return param_info.param.suite_name + "_" +
         std::to_string(param_info.param.index);
}

// Instantiate the test suite, creating one test case for each golden case
// loaded and providing it as a parameter.
INSTANTIATE_TEST_SUITE_P(Parameterized,
                         TimeLimitConsistencyTest,
                         testing::ValuesIn(LoadGoldenCases()),
                         GetTestCaseName);

}  // namespace time_limit_consistency
}  // namespace chromeos

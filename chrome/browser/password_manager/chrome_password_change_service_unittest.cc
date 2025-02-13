// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_change_service.h"

#include "base/command_line.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestCase {
  using TupleT = std::tuple<bool, bool, bool>;

  explicit TestCase(TupleT configuration)
      : is_generation_available(std::get<0>(configuration)),
        is_model_execution_allowed(std::get<1>(configuration)),
        is_feature_enabled(std::get<2>(configuration)) {}

  bool expected_outcome() const {
    return is_generation_available & is_model_execution_allowed &
           is_feature_enabled;
  }

  const bool is_generation_available;
  const bool is_model_execution_allowed;
  const bool is_feature_enabled;
};

}  // namespace

class ChromePasswordChangeServiceBase {
 public:
  ChromePasswordChangeServiceBase() {
    auto feature_manager = std::make_unique<
        testing::StrictMock<password_manager::MockPasswordFeatureManager>>();
    feature_manager_ = feature_manager.get();
    change_service_ = std::make_unique<ChromePasswordChangeService>(
        &mock_affiliation_service_, &mock_optimization_service_,
        std::move(feature_manager));
  }

  ~ChromePasswordChangeServiceBase() = default;

  affiliations::MockAffiliationService& affiliation_service() {
    return mock_affiliation_service_;
  }
  MockOptimizationGuideKeyedService& mock_optimization_service() {
    return mock_optimization_service_;
  }

  password_manager::PasswordChangeServiceInterface* change_service() {
    return change_service_.get();
  }

  password_manager::MockPasswordFeatureManager* feature_manager() {
    return feature_manager_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_{
      password_manager::features::kImprovedPasswordChangeService};
  testing::StrictMock<affiliations::MockAffiliationService>
      mock_affiliation_service_;
  testing::StrictMock<MockOptimizationGuideKeyedService>
      mock_optimization_service_;
  std::unique_ptr<ChromePasswordChangeService> change_service_;
  raw_ptr<password_manager::MockPasswordFeatureManager> feature_manager_;
};

class ChromePasswordChangeServiceTest : public testing::Test,
                                        public ChromePasswordChangeServiceBase {
};

TEST_F(ChromePasswordChangeServiceTest, PasswordChangeSupportedForURL) {
  base::HistogramTester histogram_tester;
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url))
      .WillOnce(testing::Return(GURL("https://test.com/password/")));
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(change_service()->IsPasswordChangeSupported(url));
  histogram_tester.ExpectUniqueSample(
      ChromePasswordChangeService::kHasPasswordChangeUrlHistogram, true, 1);
}

TEST_F(ChromePasswordChangeServiceTest, PasswordChangeNotSupportedForUrl) {
  base::HistogramTester histogram_tester;
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url))
      .WillOnce(testing::Return(GURL()));
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(change_service()->IsPasswordChangeSupported(url));
  histogram_tester.ExpectUniqueSample(
      ChromePasswordChangeService::kHasPasswordChangeUrlHistogram, false, 1);
}

TEST_F(ChromePasswordChangeServiceTest,
       PasswordChangeNotSupportedSettingNotVisible) {
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL).Times(0);
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(change_service()->IsPasswordChangeSupported(url));
}

TEST_F(ChromePasswordChangeServiceTest,
       PasswordChangeSupportedIfCommandLineArgProvided) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPasswordChangeUrl, "https://test.com/new_password/");

  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL).Times(0);

  EXPECT_TRUE(change_service()->IsPasswordChangeSupported(url));
}

TEST_F(ChromePasswordChangeServiceTest,
       PasswordChangeSupportedIfPSLMatchedInArg) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPasswordChangeUrl, "https://test.com/new_password/");

  GURL url("https://www.test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL).Times(0);

  EXPECT_TRUE(change_service()->IsPasswordChangeSupported(url));
}

class ChromePasswordChangeServiceAvailabilityTest
    : public testing::TestWithParam<TestCase>,
      public ChromePasswordChangeServiceBase {
 public:
  ChromePasswordChangeServiceAvailabilityTest() {
    feature_list_.InitWithFeatureState(
        password_manager::features::kImprovedPasswordChangeService,
        GetParam().is_feature_enabled);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ChromePasswordChangeServiceAvailabilityTest, TestWithNoArgs) {
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(GetParam().is_generation_available));
  if (GetParam().is_generation_available) {
    EXPECT_CALL(mock_optimization_service(),
                ShouldModelExecutionBeAllowedForUser)
        .WillOnce(testing::Return(GetParam().is_model_execution_allowed));
  }

  EXPECT_EQ(change_service()->IsPasswordChangeAvailable(),
            GetParam().expected_outcome());
}

TEST_P(ChromePasswordChangeServiceAvailabilityTest, TestWithChangePwdUrlArg) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPasswordChangeUrl, "https://test.com/new_password/");

  EXPECT_CALL(*feature_manager(), IsGenerationEnabled).Times(0);
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .Times(0);

  EXPECT_TRUE(change_service()->IsPasswordChangeAvailable());
}

INSTANTIATE_TEST_SUITE_P(
    Availability,
    ChromePasswordChangeServiceAvailabilityTest,
    testing::ConvertGenerator<TestCase::TupleT>(
        testing::Combine(testing::Bool(), testing::Bool(), testing::Bool())),
    [](const ::testing::TestParamInfo<TestCase>& info) {
      std::string test_name;
      test_name +=
          info.param.is_generation_available ? "GenerationOn" : "GenerationOff";
      test_name += info.param.is_model_execution_allowed ? "ExecutionOn"
                                                         : "ExecutionOff";
      test_name += info.param.is_feature_enabled ? "FeatureOn" : "FeatureOff";
      return test_name;
    });

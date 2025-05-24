// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "privacy_sandbox_incognito_survey_service.h"

#include <string>
#include <tuple>

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/privacy_sandbox/incognito/privacy_sandbox_incognito_features.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace privacy_sandbox {

namespace {

using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::ContainerEq;
using ::testing::Eq;
using ::testing::Return;

using ::privacy_sandbox::PrivacySandboxIncognitoSurveyService;

using ActSurveyStatus =
    ::privacy_sandbox::PrivacySandboxIncognitoSurveyService::ActSurveyStatus;

template <size_t I, typename T>
auto RunOnceClosureAndReturn(T output) {
  return [output](auto&&... args) -> decltype(auto) {
    base::test::RunOnceClosure<I>()(args...);
    return std::move(output);
  };
}

class RandIntObserver {
 public:
  RandIntObserver() = default;

  int GetMinFromLastCall() { return last_min_; }

  int GetMaxFromLastCall() { return last_max_; }

  void SetReturn(int value) { return_ = value; }

  int RandIntCallback(int min, int max) {
    last_min_ = min;
    last_max_ = max;
    return return_;
  }

 private:
  int last_min_ = 0;
  int last_max_ = 0;
  int return_ = 0;
};

class PrivacySandboxIncognitoSurveyServiceTest : public testing::Test {
 public:
  PrivacySandboxIncognitoSurveyServiceTest() = default;

  void SetUp() override {
    original_profile_ = TestingProfile::Builder().Build();
    profile_ = CreateProfile(original_profile_.get());

    SetUpHatsService();

    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
    survey_service_ = std::make_unique<PrivacySandboxIncognitoSurveyService>(
        hats_service(), profile()->IsIncognitoProfile());
  }

  void TearDown() override {
    survey_service_.reset();
    TearDownHatsService();
    profile_ = nullptr;
    original_profile_.reset();
  }

 protected:
  virtual raw_ptr<TestingProfile> CreateProfile(
      TestingProfile* original_profile) {
    return TestingProfile::Builder().BuildIncognito(original_profile);
  }

  virtual void SetUpHatsService() {
    mock_hats_service_ = BuildMockHatsService(profile());
    ON_CALL(*hats_service(), CanShowAnySurvey(_)).WillByDefault(Return(true));
  }

  virtual void TearDownHatsService() { mock_hats_service_.reset(); }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {};
  }

  PrivacySandboxIncognitoSurveyService* survey_service() {
    return survey_service_.get();
  }
  MockHatsService* hats_service() {
    return static_cast<MockHatsService*>(mock_hats_service_.get());
  }
  TestingProfile* profile() { return profile_.get(); }

  void TriggerActSurvey() { survey_service_->MaybeShowActSurvey(nullptr); }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<TestingProfile> original_profile_;
  raw_ptr<TestingProfile> profile_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<KeyedService> mock_hats_service_;
  std::unique_ptr<PrivacySandboxIncognitoSurveyService> survey_service_;
  base::test::ScopedFeatureList feature_list_;
};

class PrivacySandboxIncognitoSurveyServiceNullHatsServiceTest
    : public PrivacySandboxIncognitoSurveyServiceTest {
  void SetUpHatsService() override {}
  void TearDownHatsService() override {}
};

class PrivacySandboxIncognitoSurveyServiceRegularProfileTest
    : public PrivacySandboxIncognitoSurveyServiceTest {
  raw_ptr<TestingProfile> CreateProfile(
      TestingProfile* original_profile) override {
    return original_profile;
  }
};

class PrivacySandboxIncognitoSurveyServiceActSurveyEnabledTest
    : public PrivacySandboxIncognitoSurveyServiceTest {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kPrivacySandboxActSurvey, {}}};
  }
};

}  // namespace

TEST_F(PrivacySandboxIncognitoSurveyServiceTest,
       IsActSurveyEnabled_DisabledByDefault) {
  EXPECT_FALSE(survey_service()->IsActSurveyEnabled());
}

TEST_F(PrivacySandboxIncognitoSurveyServiceTest,
       RecordActSurveyStatus_EmitsHistogram) {
  survey_service()->RecordActSurveyStatus(ActSurveyStatus::kFeatureDisabled);
  histogram_tester_.ExpectBucketCount("PrivacySandbox.ActSurvey.Status",
                                      ActSurveyStatus::kFeatureDisabled, 1);
  histogram_tester_.ExpectTotalCount("PrivacySandbox.ActSurvey.Status", 1);
}

TEST_F(PrivacySandboxIncognitoSurveyServiceTest,
       GetActSurveyPsd_ReturnsProperPsd) {
  std::map<std::string, std::string> expected_psd = {
      {"Survey Trigger Delay", "12345"},
  };

  EXPECT_THAT(survey_service()->GetActSurveyPsd(12345),
              ContainerEq(expected_psd));
}

TEST_F(PrivacySandboxIncognitoSurveyServiceTest,
       MaybeShowActSurvey_DisabledByDefault) {
  TriggerActSurvey();
  histogram_tester_.ExpectBucketCount("PrivacySandbox.ActSurvey.Status",
                                      ActSurveyStatus::kFeatureDisabled, 1);
}

TEST_F(PrivacySandboxIncognitoSurveyServiceActSurveyEnabledTest,
       MaybeShowActSurvey_EmitsSurveyShownHistogram) {
  EXPECT_CALL(*hats_service(), LaunchDelayedSurveyForWebContents)
      .WillOnce(RunOnceClosureAndReturn<6>(true));  // run the success callback
  TriggerActSurvey();
  histogram_tester_.ExpectBucketCount("PrivacySandbox.ActSurvey.Status",
                                      ActSurveyStatus::kSurveyShown, 1);
}

TEST_F(PrivacySandboxIncognitoSurveyServiceActSurveyEnabledTest,
       MaybeShowActSurvey_EmitsSurveyLaunchedFailedHistogram) {
  EXPECT_CALL(*hats_service(), LaunchDelayedSurveyForWebContents)
      .WillOnce(RunOnceClosureAndReturn<7>(true));  // run the failure callback
  TriggerActSurvey();
  histogram_tester_.ExpectBucketCount("PrivacySandbox.ActSurvey.Status",
                                      ActSurveyStatus::kSurveyLaunchFailed, 1);
}

class PrivacySandboxIncognitoSurveyServiceRegularProfileActSurveyTest
    : public PrivacySandboxIncognitoSurveyServiceRegularProfileTest {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kPrivacySandboxActSurvey, {}}};
  }
};

TEST_F(PrivacySandboxIncognitoSurveyServiceRegularProfileActSurveyTest,
       MaybeShowActSurvey_NotLaunchedInRegularProfile) {
  TriggerActSurvey();
  histogram_tester_.ExpectBucketCount("PrivacySandbox.ActSurvey.Status",
                                      ActSurveyStatus::kNonIncognitoProfile, 1);
}

class PrivacySandboxIncognitoSurveyServiceActSurveyWithDelayTest
    : public PrivacySandboxIncognitoSurveyServiceTest {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kPrivacySandboxActSurvey,
             {
                 {kPrivacySandboxActSurveyDelay.name, "16s"},
             }}};
  }
};

TEST_F(PrivacySandboxIncognitoSurveyServiceActSurveyWithDelayTest,
       MaybeShowActSurvey_LaunchedWithDelay) {
  std::map<std::string, std::string> expected_psd = {
      {"Survey Trigger Delay", "16000"}};
  EXPECT_CALL(
      *hats_service(),
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerPrivacySandboxActSurvey, _, 16000, _, expected_psd,
          HatsService::NavigationBehaviour::REQUIRE_SAME_DOCUMENT, _, _, _, _));
  TriggerActSurvey();
  testing::Mock::VerifyAndClearExpectations(hats_service());
}

class PrivacySandboxIncognitoSurveyServiceNullHatsServiceActSurveyTest
    : public PrivacySandboxIncognitoSurveyServiceNullHatsServiceTest {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kPrivacySandboxActSurvey, {}}};
  }
};

TEST_F(PrivacySandboxIncognitoSurveyServiceNullHatsServiceActSurveyTest,
       MaybeShowActSurvey_EmitsHatsServiceFailedHistogram) {
  TriggerActSurvey();
  histogram_tester_.ExpectBucketCount("PrivacySandbox.ActSurvey.Status",
                                      ActSurveyStatus::kHatsServiceFailed, 1);
}

class PrivacySandboxIncognitoSurveyServiceActSurveyDelayTest
    : public PrivacySandboxIncognitoSurveyServiceTest,
      public testing::WithParamInterface<
          testing::tuple<std::string, base::TimeDelta>> {
 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kPrivacySandboxActSurvey,
             {
                 {kPrivacySandboxActSurveyDelayRandomize.name, "false"},
                 {kPrivacySandboxActSurveyDelay.name, GetDelay()},
                 {kPrivacySandboxActSurveyDelayMin.name, "10s"},
                 {kPrivacySandboxActSurveyDelayMax.name, "20s"},
             }}};
  }

  std::string GetDelay() { return testing::get<0>(GetParam()); }

  base::TimeDelta GetExpectedDelay() { return testing::get<1>(GetParam()); }
};

TEST_P(PrivacySandboxIncognitoSurveyServiceActSurveyDelayTest,
       CalculateActSurveyDelay_ProperlyCalculatesDelay) {
  auto delay = survey_service()->CalculateActSurveyDelay();
  EXPECT_EQ(delay, GetExpectedDelay());
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxIncognitoSurveyServiceActSurveyDelayTest,
    PrivacySandboxIncognitoSurveyServiceActSurveyDelayTest,
    testing::Values(testing::make_tuple("0s", base::Seconds(0)),
                    testing::make_tuple("10s", base::Seconds(10)),
                    testing::make_tuple("1234ms", base::Milliseconds(1234))));

class PrivacySandboxIncognitoSurveyServiceActSurveyDelayRandomizationTest
    : public PrivacySandboxIncognitoSurveyServiceTest,
      public testing::WithParamInterface<
          testing::
              tuple<std::string, std::string, int, int, int, base::TimeDelta>> {
 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kPrivacySandboxActSurvey,
             {
                 {kPrivacySandboxActSurveyDelayRandomize.name, "true"},
                 {kPrivacySandboxActSurveyDelay.name, "42s"},
                 {kPrivacySandboxActSurveyDelayMin.name, GetDelayMin()},
                 {kPrivacySandboxActSurveyDelayMax.name, GetDelayMax()},
             }}};
  }

  std::string GetDelayMin() { return testing::get<0>(GetParam()); }

  std::string GetDelayMax() { return testing::get<1>(GetParam()); }

  int GetExpectedDelayMin() { return testing::get<2>(GetParam()); }

  int GetExpectedDelayMax() { return testing::get<3>(GetParam()); }

  int GetMockedDelay() { return testing::get<4>(GetParam()); }

  base::TimeDelta GetExpectedDelay() { return testing::get<5>(GetParam()); }
};

TEST_P(PrivacySandboxIncognitoSurveyServiceActSurveyDelayRandomizationTest,
       CalculateActSurveyDelay_ProperlyCalculatesRandomizedDelay) {
  RandIntObserver rand_int_observer;
  survey_service()->SetRandIntCallbackForTesting(base::BindRepeating(
      &RandIntObserver::RandIntCallback, base::Unretained(&rand_int_observer)));

  rand_int_observer.SetReturn(GetMockedDelay());
  auto delay = survey_service()->CalculateActSurveyDelay();

  EXPECT_EQ(delay, GetExpectedDelay());
  EXPECT_EQ(rand_int_observer.GetMinFromLastCall(), GetExpectedDelayMin());
  EXPECT_EQ(rand_int_observer.GetMaxFromLastCall(), GetExpectedDelayMax());
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxIncognitoSurveyServiceActSurveyDelayRandomizationTest,
    PrivacySandboxIncognitoSurveyServiceActSurveyDelayRandomizationTest,
    testing::Values(
        testing::make_tuple("0s", "10s", 0, 10, 5, base::Seconds(5)),
        testing::make_tuple("0", "1m1s", 0, 61, 60, base::Seconds(60)),
        testing::make_tuple("1234ms", "12345ms", 1, 12, 1, base::Seconds(1)),
        testing::make_tuple("10s", "5s", 5, 10, 7, base::Seconds(7))));

}  // namespace privacy_sandbox

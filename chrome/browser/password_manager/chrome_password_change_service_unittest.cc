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
#include "chrome/test/base/testing_browser_process.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/mock_password_manager_settings_service.h"
#include "components/password_manager/core/browser/password_manager_switches.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/service/test_variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestCase {
  using TupleT = std::tuple<bool, bool, bool, bool, bool>;

  explicit TestCase(TupleT configuration)
      : is_generation_available(std::get<0>(configuration)),
        is_model_execution_allowed(std::get<1>(configuration)),
        is_saving_allowed(std::get<2>(configuration)),
        is_disabled_by_policy(std::get<3>(configuration)),
        is_feature_enabled(std::get<4>(configuration)) {}

  bool expected_outcome() const {
#if BUILDFLAG(IS_ANDROID)
    return false;
#else
    return is_generation_available & is_model_execution_allowed &
           is_saving_allowed & is_feature_enabled & !is_disabled_by_policy;
#endif  // BUILDFLAG(IS_ANDROID)
  }

  const bool is_generation_available;
  const bool is_model_execution_allowed;
  const bool is_saving_allowed;
  const bool is_disabled_by_policy;
  const bool is_feature_enabled;
};

}  // namespace

class ChromePasswordChangeServiceBase {
 public:
  ChromePasswordChangeServiceBase() {
    prefs()->registry()->RegisterIntegerPref(
        optimization_guide::prefs::GetSettingEnabledPrefName(
            optimization_guide::UserVisibleFeatureKey::
                kPasswordChangeSubmission),
        /*default_value=*/0);
    prefs()->registry()->RegisterIntegerPref(
        optimization_guide::prefs::
            kAutomatedPasswordChangeEnterprisePolicyAllowed,
        /*default_value=*/0);
    prefs()->registry()->RegisterIntegerPref(
        password_manager::prefs::kTotalPasswordsAvailableForAccount,
        /*default_value=*/1);
    prefs()->registry()->RegisterIntegerPref(
        password_manager::prefs::kTotalPasswordsAvailableForProfile,
        /*default_value=*/0);
    prefs()->registry()->RegisterTimePref(
        password_manager::prefs::kLastNegativePasswordChangeTimestamp,
        /*default_value=*/base::Time());
    auto feature_manager = std::make_unique<
        testing::StrictMock<password_manager::MockPasswordFeatureManager>>();
    feature_manager_ = feature_manager.get();
    change_service_ = std::make_unique<ChromePasswordChangeService>(
        &prefs_, &mock_affiliation_service_, &mock_optimization_service_,
        &settings_service_, std::move(feature_manager), &log_router_);
  }

  ~ChromePasswordChangeServiceBase() = default;

  affiliations::MockAffiliationService& affiliation_service() {
    return mock_affiliation_service_;
  }
  MockOptimizationGuideKeyedService& mock_optimization_service() {
    return mock_optimization_service_;
  }
  password_manager::MockPasswordManagerSettingsService& settings_service() {
    return settings_service_;
  }

  password_manager::PasswordChangeServiceInterface* change_service() {
    return change_service_.get();
  }

  password_manager::MockPasswordFeatureManager* feature_manager() {
    return feature_manager_;
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_{
      password_manager::features::kImprovedPasswordChangeService};
  autofill::LogRouter log_router_;
  TestingPrefServiceSimple prefs_;
  testing::StrictMock<affiliations::MockAffiliationService>
      mock_affiliation_service_;
  testing::StrictMock<MockOptimizationGuideKeyedService>
      mock_optimization_service_;
  testing::StrictMock<password_manager::MockPasswordManagerSettingsService>
      settings_service_;
  std::unique_ptr<ChromePasswordChangeService> change_service_;
  raw_ptr<password_manager::MockPasswordFeatureManager> feature_manager_;
};

class ChromePasswordChangeServiceTest : public testing::TestWithParam<bool>,
                                        public ChromePasswordChangeServiceBase {
 public:
  ChromePasswordChangeServiceTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{password_manager::features::kReduceRequirementsForPasswordChange,
          GetParam()}});
    variations::TestVariationsService::RegisterPrefs(prefs()->registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        prefs(), &enabled_state_provider_, std::wstring(), base::FilePath());
    variations_service_ = std::make_unique<variations::TestVariationsService>(
        prefs(), metrics_state_manager_.get());
  }

  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetVariationsService(
        variations_service_.get());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  metrics::TestEnabledStateProvider enabled_state_provider_{/*consent=*/false,
                                                            /*enabled=*/false};
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
};

#if !BUILDFLAG(IS_ANDROID)
TEST_P(ChromePasswordChangeServiceTest, PasswordChangeSupportedForURL) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "us");

  base::HistogramTester histogram_tester;
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url))
      .WillOnce(testing::Return(GURL("https://test.com/password/")));
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(settings_service(), IsSettingEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(change_service()->IsPasswordChangeSupported(
      url, autofill::LanguageCode("en")));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeAvailability",
      PasswordChangeAvailability::kAvailable, 1);
}

TEST_P(ChromePasswordChangeServiceTest, NoChangePasswordUrl) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "us");

  base::HistogramTester histogram_tester;
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url))
      .WillOnce(testing::Return(GURL()));
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(settings_service(), IsSettingEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(change_service()->IsPasswordChangeSupported(
      url, autofill::LanguageCode("en")));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeAvailability",
      PasswordChangeAvailability::kNotSupportedSite, 1);
}

TEST_P(ChromePasswordChangeServiceTest, DifferentCountry) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "in");

  base::HistogramTester histogram_tester;
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url)).Times(0);
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(settings_service(), IsSettingEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(true));
  if (base::FeatureList::IsEnabled(
          password_manager::features::kReduceRequirementsForPasswordChange)) {
    EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url))
        .WillOnce(testing::Return(GURL("https://test.com/password/")));
    EXPECT_TRUE(change_service()->IsPasswordChangeSupported(
        url, autofill::LanguageCode("en")));
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.PasswordChangeAvailability",
        PasswordChangeAvailability::kAvailable, 1);
  } else {
    EXPECT_FALSE(change_service()->IsPasswordChangeSupported(
        url, autofill::LanguageCode("en")));
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.PasswordChangeAvailability",
        PasswordChangeAvailability::kUnsupportedCountryCode, 1);
  }
}

TEST_P(ChromePasswordChangeServiceTest, DifferentLanguage) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "us");

  base::HistogramTester histogram_tester;
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url)).Times(0);
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(settings_service(), IsSettingEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(true));
  if (base::FeatureList::IsEnabled(
          password_manager::features::kReduceRequirementsForPasswordChange)) {
    EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url))
        .WillOnce(testing::Return(GURL("https://test.com/password/")));
    EXPECT_TRUE(change_service()->IsPasswordChangeSupported(
        url, autofill::LanguageCode("ru")));
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.PasswordChangeAvailability",
        PasswordChangeAvailability::kAvailable, 1);
  } else {
    EXPECT_CALL(affiliation_service(), GetChangePasswordURL).Times(0);
    EXPECT_FALSE(change_service()->IsPasswordChangeSupported(
        url, autofill::LanguageCode("ru")));
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.PasswordChangeAvailability",
        PasswordChangeAvailability::kUnsupportedLanguage, 1);
  }
}

TEST_P(ChromePasswordChangeServiceTest,
       PasswordChangeNotSupportedSettingNotVisible) {
  base::HistogramTester histogram_tester;
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL).Times(0);
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(change_service()->IsPasswordChangeSupported(
      url, autofill::LanguageCode("en")));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeAvailability",
      PasswordChangeAvailability::kModelExecutionNotAllowed, 1);
}

TEST_P(ChromePasswordChangeServiceTest,
       PasswordChangeSupportedIfCommandLineArgProvided) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      password_manager::kPasswordChangeUrl, "https://test.com/new_password/");

  base::HistogramTester histogram_tester;
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL).Times(0);

  EXPECT_TRUE(change_service()->IsPasswordChangeSupported(
      url, autofill::LanguageCode("en")));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeAvailability",
      PasswordChangeAvailability::kAvailable, 1);
}

TEST_P(ChromePasswordChangeServiceTest,
       PasswordChangeSupportedIfPSLMatchedInArg) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      password_manager::kPasswordChangeUrl, "https://test.com/new_password/");

  base::HistogramTester histogram_tester;
  GURL url("https://www.test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL).Times(0);

  EXPECT_TRUE(change_service()->IsPasswordChangeSupported(
      url, autofill::LanguageCode("en")));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeAvailability",
      PasswordChangeAvailability::kAvailable, 1);
}

TEST_P(ChromePasswordChangeServiceTest,
       PasswordChangeNotSupportedIfNoSavedPasswords) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "us");
  base::HistogramTester histogram_tester;

  GURL url("https://test.com/");
  prefs()->SetInteger(
      password_manager::prefs::kTotalPasswordsAvailableForAccount, 0);
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(settings_service(), IsSettingEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(true));

  EXPECT_FALSE(change_service()->IsPasswordChangeSupported(
      url, autofill::LanguageCode("en")));
}

TEST_P(ChromePasswordChangeServiceTest, PasswordChangeThrottledAfterFailure) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "us");

  base::HistogramTester histogram_tester;
  GURL url("https://test.com/");
  EXPECT_CALL(affiliation_service(), GetChangePasswordURL(url))
      .WillRepeatedly(testing::Return(GURL("https://test.com/password/")));
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(settings_service(), IsSettingEnabled)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillRepeatedly(testing::Return(true));
  EXPECT_TRUE(change_service()->IsPasswordChangeSupported(
      url, autofill::LanguageCode("en")));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeAvailability",
      PasswordChangeAvailability::kAvailable, 1);

  prefs()->SetTime(
      password_manager::prefs::kLastNegativePasswordChangeTimestamp,
      base::Time::Now());
  EXPECT_FALSE(change_service()->IsPasswordChangeSupported(
      url, autofill::LanguageCode("en")));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.PasswordChangeAvailability"),
              testing::ElementsAre(
                  base::Bucket(PasswordChangeAvailability::kAvailable, 1),
                  base::Bucket(PasswordChangeAvailability::kThrottled, 1)));

  AdvanceClock(password_manager::features::kPasswordChangeThrottleTime.Get() +
               base::Seconds(1));

  EXPECT_TRUE(change_service()->IsPasswordChangeSupported(
      url, autofill::LanguageCode("en")));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.PasswordChangeAvailability"),
              testing::ElementsAre(
                  base::Bucket(PasswordChangeAvailability::kAvailable, 2),
                  base::Bucket(PasswordChangeAvailability::kThrottled, 1)));
}

INSTANTIATE_TEST_SUITE_P(, ChromePasswordChangeServiceTest, testing::Bool());

#endif  // !BUILDFLAG(IS_ANDROID)

class ChromePasswordChangeServiceAvailabilityTest
    : public testing::TestWithParam<TestCase>,
      public ChromePasswordChangeServiceBase {
 public:
  ChromePasswordChangeServiceAvailabilityTest() {
    feature_list_.InitWithFeatureState(
        password_manager::features::kImprovedPasswordChangeService,
        GetParam().is_feature_enabled);
    if (GetParam().is_disabled_by_policy) {
      constexpr int kPolicyDisabled = base::to_underlying(
          optimization_guide::model_execution::prefs::
              ModelExecutionEnterprisePolicyValue::kDisable);
      prefs()->SetInteger(optimization_guide::prefs::
                              kAutomatedPasswordChangeEnterprisePolicyAllowed,
                          kPolicyDisabled);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ChromePasswordChangeServiceAvailabilityTest, TestWithNoArgs) {
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(GetParam().is_generation_available));
  if (GetParam().is_generation_available) {
    EXPECT_CALL(mock_optimization_service(),
                ShouldModelExecutionBeAllowedForUser)
        .WillOnce(testing::Return(GetParam().is_model_execution_allowed));
    if (GetParam().is_model_execution_allowed) {
      EXPECT_CALL(
          settings_service(),
          IsSettingEnabled(
              password_manager::PasswordManagerSetting::kOfferToSavePasswords))
          .WillOnce(testing::Return(GetParam().is_saving_allowed));
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  EXPECT_EQ(change_service()->IsPasswordChangeAvailable(),
            GetParam().expected_outcome());
}

TEST_P(ChromePasswordChangeServiceAvailabilityTest, TestWithChangePwdUrlArg) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      password_manager::kPasswordChangeUrl, "https://test.com/new_password/");

  EXPECT_CALL(*feature_manager(), IsGenerationEnabled).Times(0);
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .Times(0);
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(change_service()->IsPasswordChangeAvailable());
#else
  EXPECT_FALSE(change_service()->IsPasswordChangeAvailable());
#endif  // !BUILDFLAG(IS_ANDROID)
}

TEST_P(ChromePasswordChangeServiceAvailabilityTest,
       UserEnrollmentIntoPasswordChangeServiceyWithPrefOff) {
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled).Times(0);
  EXPECT_CALL(mock_optimization_service(), ShouldModelExecutionBeAllowedForUser)
      .Times(0);
  EXPECT_CALL(settings_service(), IsSettingEnabled).Times(0);
  // Always false because pref is not set.
  EXPECT_FALSE(static_cast<ChromePasswordChangeService*>(change_service())
                   ->UserIsActivePasswordChangeUser());
}

TEST_P(ChromePasswordChangeServiceAvailabilityTest,
       UserEnrollmentIntoPasswordChangeServiceWithPrefOn) {
  prefs()->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kPasswordChangeSubmission),
      1);
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(*feature_manager(), IsGenerationEnabled)
      .WillOnce(testing::Return(GetParam().is_generation_available));
  if (GetParam().is_generation_available) {
    EXPECT_CALL(mock_optimization_service(),
                ShouldModelExecutionBeAllowedForUser)
        .WillOnce(testing::Return(GetParam().is_model_execution_allowed));
    if (GetParam().is_model_execution_allowed) {
      EXPECT_CALL(
          settings_service(),
          IsSettingEnabled(
              password_manager::PasswordManagerSetting::kOfferToSavePasswords))
          .WillOnce(testing::Return(GetParam().is_saving_allowed));
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(static_cast<ChromePasswordChangeService*>(change_service())
                ->UserIsActivePasswordChangeUser(),
            GetParam().expected_outcome());
}

INSTANTIATE_TEST_SUITE_P(
    Availability,
    ChromePasswordChangeServiceAvailabilityTest,
    testing::ConvertGenerator<TestCase::TupleT>(
        testing::Combine(testing::Bool(),
                         testing::Bool(),
                         testing::Bool(),
                         testing::Bool(),
                         testing::Bool())),
    [](const ::testing::TestParamInfo<TestCase>& info) {
      std::string test_name;
      test_name +=
          info.param.is_generation_available ? "GenerationOn" : "GenerationOff";
      test_name += info.param.is_model_execution_allowed ? "ExecutionOn"
                                                         : "ExecutionOff";
      test_name += info.param.is_saving_allowed ? "SavingOn" : "SavingOff";
      test_name += info.param.is_disabled_by_policy ? "DisabledByPolicy" : "";
      test_name += info.param.is_feature_enabled ? "FeatureOn" : "FeatureOff";
      return test_name;
    });

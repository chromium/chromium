// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_metrics.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicProviderMetricsTest : public testing::TestWithParam<bool> {
 protected:
  GlicProviderMetricsTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeatureWithParameters(
          features::kGeic,
          {{"geic-guest-url", "https://business.gemini.google/cli-panel"}});
    } else {
      feature_list_.InitAndDisableFeature(features::kGeic);
    }
  }

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.push_back(
        {GlicKeyedServiceFactory::GetInstance(),
         base::BindRepeating(
             [](ProfileManager* pm, GlicProfileManager* gpm,
                content::BrowserContext* context)
                 -> std::unique_ptr<KeyedService> {
               return std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
                   context,
                   IdentityManagerFactory::GetForProfile(
                       Profile::FromBrowserContext(context)),
                   pm, gpm,
                   /*contextual_cueing_service=*/nullptr,
                   /*actor_keyed_service=*/nullptr);
             },
             testing_profile_manager_.profile_manager(),
             &glic_profile_manager_)});

    profile_ = testing_profile_manager_.CreateTestingProfile(
        "profile", std::move(factories));
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    ASSERT_TRUE(GlicKeyedServiceFactory::GetGlicKeyedService(profile_.get(),
                                                             /*create=*/true));
  }

  GlicEnabling::ScopedBypassEnablementChecksForTesting bypass_enablement_;
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  GlicProfileManager glic_profile_manager_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_P(GlicProviderMetricsTest, Infix) {
  bool is_enterprise = GetParam();
  std::string provider_str = is_enterprise ? "Enterprise" : "Consumer";

  EXPECT_EQ(provider_str, GlicMetricReporter(profile_.get()).infix());
}

TEST_P(GlicProviderMetricsTest, Reporter) {
  bool is_enterprise = GetParam();
  std::string provider_str = is_enterprise ? "Enterprise" : "Consumer";
  GlicMetricReporter reporter(profile_.get());
  base::HistogramTester histogram_tester;

  EXPECT_EQ(provider_str, reporter.infix());

  // UmaHistogramBoolean
  reporter.RecordBoolean("Boolean", true);
  histogram_tester.ExpectUniqueSample("Glic.Boolean", true, 1);
  histogram_tester.ExpectUniqueSample("Glic." + provider_str + ".Boolean", true,
                                      1);

  // UmaHistogramCounts100
  reporter.RecordCounts100("Counts100", 50);
  histogram_tester.ExpectUniqueSample("Glic.Counts100", 50, 1);
  histogram_tester.ExpectUniqueSample("Glic." + provider_str + ".Counts100", 50,
                                      1);

  // UmaHistogramCounts1000
  reporter.RecordCounts1000("Counts1000", 500);
  histogram_tester.ExpectUniqueSample("Glic.Counts1000", 500, 1);
  histogram_tester.ExpectUniqueSample("Glic." + provider_str + ".Counts1000",
                                      500, 1);

  // UmaHistogramCounts10000
  reporter.RecordCounts10000("Counts10000", 5000);
  histogram_tester.ExpectUniqueSample("Glic.Counts10000", 5000, 1);
  histogram_tester.ExpectUniqueSample("Glic." + provider_str + ".Counts10000",
                                      5000, 1);

  // UmaHistogramCustomTimes
  reporter.RecordCustomTimes("CustomTimes", base::Seconds(5), base::Seconds(1),
                             base::Seconds(10), 10);
  histogram_tester.ExpectUniqueTimeSample("Glic.CustomTimes", base::Seconds(5),
                                          1);
  histogram_tester.ExpectUniqueTimeSample(
      "Glic." + provider_str + ".CustomTimes", base::Seconds(5), 1);

  // UmaHistogramEnumeration (2 args)
  enum class TestEnum { kOne = 1, kTwo = 2, kMaxValue = kTwo };
  reporter.RecordEnumeration("Enum2", TestEnum::kOne);
  histogram_tester.ExpectUniqueSample("Glic.Enum2", TestEnum::kOne, 1);
  histogram_tester.ExpectUniqueSample("Glic." + provider_str + ".Enum2",
                                      TestEnum::kOne, 1);

  // UmaHistogramEnumeration (3 args)
  reporter.RecordEnumeration("Enum3", TestEnum::kTwo, TestEnum::kTwo);
  histogram_tester.ExpectUniqueSample("Glic.Enum3", TestEnum::kTwo, 1);
  histogram_tester.ExpectUniqueSample("Glic." + provider_str + ".Enum3",
                                      TestEnum::kTwo, 1);

  // UmaHistogramExactLinear
  reporter.RecordExactLinear("ExactLinear", 10, 20);
  histogram_tester.ExpectUniqueSample("Glic.ExactLinear", 10, 1);
  histogram_tester.ExpectUniqueSample("Glic." + provider_str + ".ExactLinear",
                                      10, 1);

  // UmaHistogramLongTimes
  reporter.RecordLongTimes("LongTimes", base::Seconds(50));
  histogram_tester.ExpectUniqueTimeSample("Glic.LongTimes", base::Seconds(50),
                                          1);
  histogram_tester.ExpectUniqueTimeSample("Glic." + provider_str + ".LongTimes",
                                          base::Seconds(50), 1);

  // UmaHistogramMediumTimes
  reporter.RecordMediumTimes("MediumTimes", base::Seconds(15));
  histogram_tester.ExpectUniqueTimeSample("Glic.MediumTimes", base::Seconds(15),
                                          1);
  histogram_tester.ExpectUniqueTimeSample(
      "Glic." + provider_str + ".MediumTimes", base::Seconds(15), 1);

  // UmaHistogramMemoryLargeMB
  reporter.RecordMemoryLargeMB("MemoryLargeMB", 1024);
  histogram_tester.ExpectUniqueSample("Glic.MemoryLargeMB", 1024, 1);
  histogram_tester.ExpectUniqueSample("Glic." + provider_str + ".MemoryLargeMB",
                                      1024, 1);

  // UmaHistogramSparse
  reporter.RecordSparse("Sparse", 99);
  histogram_tester.ExpectUniqueSample("Glic.Sparse", 99, 1);
  histogram_tester.ExpectUniqueSample("Glic." + provider_str + ".Sparse", 99,
                                      1);

  // UmaHistogramTimes
  reporter.RecordTimes("Times", base::Seconds(2));
  histogram_tester.ExpectUniqueTimeSample("Glic.Times", base::Seconds(2), 1);
  histogram_tester.ExpectUniqueTimeSample("Glic." + provider_str + ".Times",
                                          base::Seconds(2), 1);
}

TEST_P(GlicProviderMetricsTest, ConsumerFallback) {
  EXPECT_EQ("Consumer", GlicMetricReporter(nullptr).infix());

  // A profile without a GlicKeyedService still resolves its latched provider.
  TestingProfile* no_service_profile =
      testing_profile_manager_.CreateTestingProfile(
          "no_service",
          {TestingProfile::TestingFactory{
              GlicKeyedServiceFactory::GetInstance(),
              base::BindRepeating(
                  [](content::BrowserContext*)
                      -> std::unique_ptr<KeyedService> { return nullptr; })}});
  ASSERT_FALSE(GlicKeyedService::Get(no_service_profile));

  std::string provider_str = GetParam() ? "Enterprise" : "Consumer";
  EXPECT_EQ(provider_str, GlicMetricReporter(no_service_profile).infix());

  GlicMetricReporter reporter(no_service_profile);
  base::HistogramTester histogram_tester;
  reporter.RecordBoolean("Fallback", true);
  histogram_tester.ExpectUniqueSample("Glic." + provider_str + ".Fallback",
                                      true, 1);
}

INSTANTIATE_TEST_SUITE_P(All, GlicProviderMetricsTest, testing::Bool());

}  // namespace glic

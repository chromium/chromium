// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

TEST(GlicProfileEnablementTest,
     RecordProfileIneligibilityMetricsAtStartup_FeatureDisabled) {
  content::BrowserTaskEnvironment task_environment;
  base::HistogramTester histograms;
  TestingProfile profile;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlic);

  GlicEnabling::RecordProfileIneligibilityMetricsAtStartup(&profile);

  histograms.ExpectUniqueSample("Glic.ProfileEnablement.IsEnabled.Startup",
                                false, 1);
  histograms.ExpectBucketCount(
      "Glic.ProfileEnablement.DisabledReason.Startup",
      GlicEnabling::ProfileEnablement::Reason::kFeatureDisabled, 1);
}

TEST(GlicProfileEnablementTest,
     RecordProfileIneligibilityMetricsAtStartup_NotRegularProfile) {
  content::BrowserTaskEnvironment task_environment;
  base::HistogramTester histograms;

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  GlicEnabling::RecordProfileIneligibilityMetricsAtStartup(profile.get());

  histograms.ExpectUniqueSample("Glic.ProfileEnablement.IsEnabled.Startup",
                                false, 1);
  histograms.ExpectBucketCount(
      "Glic.ProfileEnablement.DisabledReason.Startup",
      GlicEnabling::ProfileEnablement::Reason::kNotRegularProfile, 1);
}

TEST(GlicProfileEnablementTest, RecordMetrics) {
  base::HistogramTester histograms;
  GlicEnabling::ProfileEnablement enablement;

  enablement.RecordStartupMetrics();

  histograms.ExpectUniqueSample("Glic.ProfileEnablement.IsEnabled.Startup",
                                true, 1);
  histograms.ExpectUniqueSample("Glic.ProfileEnablement.IsConsented.Startup",
                                true, 1);
  histograms.ExpectUniqueSample(
      "Glic.ProfileEnablement.EligibleForLive.Startup", true, 1);
  histograms.ExpectUniqueSample(
      "Glic.ProfileEnablement.IsPrimaryAccountFullySignedIn.Startup", true, 1);
  histograms.ExpectTotalCount("Glic.ProfileEnablement.DisabledReason.Startup",
                              0);

  // Set some reasons for disablement
  enablement.feature_disabled = true;
  enablement.not_rolled_out = true;
  enablement.not_consented = true;
  enablement.live_disallowed = true;
  enablement.primary_account_not_fully_signed_in = true;

  enablement.RecordSteadyStateMetrics();

  histograms.ExpectUniqueSample("Glic.ProfileEnablement.IsEnabled.SteadyState",
                                false, 1);
  histograms.ExpectBucketCount(
      "Glic.ProfileEnablement.DisabledReason.SteadyState",
      GlicEnabling::ProfileEnablement::Reason::kFeatureDisabled, 1);
  histograms.ExpectBucketCount(
      "Glic.ProfileEnablement.DisabledReason.SteadyState",
      GlicEnabling::ProfileEnablement::Reason::kNotRolledOut, 1);
  histograms.ExpectTotalCount(
      "Glic.ProfileEnablement.DisabledReason.SteadyState", 2);

  // Sub-features
  histograms.ExpectUniqueSample(
      "Glic.ProfileEnablement.IsConsented.SteadyState", false, 1);
  // EligibleForLive is false because feature_disabled is true
  // (IsProfileEligible() is false)
  histograms.ExpectUniqueSample(
      "Glic.ProfileEnablement.EligibleForLive.SteadyState", false, 1);
  histograms.ExpectUniqueSample(
      "Glic.ProfileEnablement.IsPrimaryAccountFullySignedIn.SteadyState", false,
      1);
}

}  // namespace glic

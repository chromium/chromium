// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_confirmation.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"

namespace privacy_sandbox {
namespace {

struct PrivacySandboxConfirmationTestData {
  // Inputs
  std::vector<base::test::FeatureRefAndParams> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  std::string variation_country;
  // Expectations
  bool expect_required;
  bool expect_mismatch_histogram_true;
  bool expect_mismatch_histogram_false;
};

// TODO(b/342221188): Add histogram tests for PrivacySandbox.NoticeRequirement.*
// histograms.
class PrivacySandboxConfirmationTestBase
    : public InProcessBrowserTest,
      public testing::WithParamInterface<PrivacySandboxConfirmationTestData> {
 public:
  PrivacySandboxConfirmationTestBase() {
    // Disabling the field trial testing config explicitly as some tests here
    // are specifically for when the feature isn't overridden.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        variations::switches::kDisableFieldTrialTestingConfig);
    feature_list_.InitWithFeaturesAndParameters(GetParam().enabled_features,
                                                GetParam().disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PrivacySandboxConsentConfirmationTest
    : public PrivacySandboxConfirmationTestBase {};

base::test::FeatureRefAndParams ConsentFeature() {
  return {kPrivacySandboxSettings4,
          {{kPrivacySandboxSettings4ConsentRequiredName, "true"}}};
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxConsentConfirmationTest, ConsentTest) {
  // Setup
  base::HistogramTester histogram_tester;
  g_browser_process->variations_service()->OverrideStoredPermanentCountry(
      GetParam().variation_country);

  EXPECT_EQ(IsConsentRequired(), GetParam().expect_required);
  histogram_tester.ExpectBucketCount(
      "Settings.PrivacySandbox.ConsentCheckIsMismatched", true,
      GetParam().expect_mismatch_histogram_true);
  histogram_tester.ExpectBucketCount(
      "Settings.PrivacySandbox.ConsentCheckIsMismatched", false,
      GetParam().expect_mismatch_histogram_false);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrivacySandboxConsentConfirmationTest,
    testing::Values(
        // 1. GB
        // 1.1 GB - Feature Overridden, Consent param set to true.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {ConsentFeature()},
            .variation_country = "gb",
            // Expectations
            .expect_required = true,
            .expect_mismatch_histogram_false = true,
        },
        // 1.2 GB - Feature Overridden. consent param not set.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {{kPrivacySandboxSettings4, {{}}}},
            .variation_country = "gb",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_true = true,
        },
        // 1.3 GB - Feature Explicitly Disabled.
        PrivacySandboxConfirmationTestData{
            .disabled_features = {kPrivacySandboxSettings4},
            .variation_country = "gb",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_true = true,
        },
        // 1.4 GB - Feature Not Set.
        PrivacySandboxConfirmationTestData{
            .variation_country = "gb",
            // Expectations
            .expect_required = true,
        },
        // 2. US
        // 2.1 US - Feature Overridden, Consent param set to true.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {ConsentFeature()},
            .variation_country = "us",
            // Expectations
            .expect_required = true,
            .expect_mismatch_histogram_true = true,
        },
        // 2.2 US - Feature Overridden. consent param not set.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {{kPrivacySandboxSettings4, {{}}}},
            .variation_country = "us",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_false = true,
        },
        // 2.3 US - Feature Explicitly Disabled.
        PrivacySandboxConfirmationTestData{
            .disabled_features = {kPrivacySandboxSettings4},
            .variation_country = "us",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_false = true,
        },
        // 2.4 US - Feature Not Set.
        PrivacySandboxConfirmationTestData{
            .variation_country = "us",
            // Expectations
            .expect_required = false,
        }));

class PrivacySandboxNoticeConfirmationTest
    : public PrivacySandboxConfirmationTestBase {};

base::test::FeatureRefAndParams NoticeFeature() {
  return {kPrivacySandboxSettings4,
          {{kPrivacySandboxSettings4NoticeRequiredName, "true"}}};
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxNoticeConfirmationTest, NoticeTest) {
  // Setup
  base::HistogramTester histogram_tester;
  g_browser_process->variations_service()->OverrideStoredPermanentCountry(
      GetParam().variation_country);
  EXPECT_EQ(IsNoticeRequired(), GetParam().expect_required);
  histogram_tester.ExpectBucketCount(
      "Settings.PrivacySandbox.NoticeCheckIsMismatched", true,
      GetParam().expect_mismatch_histogram_true);
  histogram_tester.ExpectBucketCount(
      "Settings.PrivacySandbox.NoticeCheckIsMismatched", false,
      GetParam().expect_mismatch_histogram_false);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrivacySandboxNoticeConfirmationTest,
    testing::Values(
        // 1. GB
        // 1.1 GB - Feature Overridden, Notice param set to true.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {NoticeFeature()},
            .variation_country = "gb",
            // Expectations
            .expect_required = true,
            .expect_mismatch_histogram_true = true,
        },
        // 1.2 GB - Feature Overridden. notice param not set.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {{kPrivacySandboxSettings4, {{}}}},
            .variation_country = "gb",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_false = true,
        },
        // 1.3 GB - Feature Explicitly Disabled.
        PrivacySandboxConfirmationTestData{
            .disabled_features = {kPrivacySandboxSettings4},
            .variation_country = "gb",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_false = true,
        },
        // 1.4 GB - Feature Not Set.
        PrivacySandboxConfirmationTestData{
            .variation_country = "gb",
            // Expectations
            .expect_required = false,
        },
        // 2. US
        // 2.1 US - Feature Overridden, notice param set to true.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {NoticeFeature()},
            .variation_country = "us",
            // Expectations
            .expect_required = true,
            .expect_mismatch_histogram_false = true,
        },
        // 2.2 US - Feature Overridden. notice param not set.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {{kPrivacySandboxSettings4, {{}}}},
            .variation_country = "us",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_true = true,
        },
        // 2.3 US - Feature Explicitly Disabled.
        PrivacySandboxConfirmationTestData{
            .disabled_features = {kPrivacySandboxSettings4},
            .variation_country = "us",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_true = true,
        },
        // 2.4 US - Feature Not Set.
        PrivacySandboxConfirmationTestData{
            .variation_country = "us",
            // Expectations
            .expect_required = true,
        },
        // 3. Empty Country
        // 3.1 Empty Country - Feature Overridden, Notice param set to true.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {NoticeFeature()},
            .variation_country = "",
            // Expectations
            .expect_required = true,
            .expect_mismatch_histogram_true = true,
        },
        // 3.2 Empty Country - Feature Overridden. notice param not set.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {{kPrivacySandboxSettings4, {{}}}},
            .variation_country = "",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_false = true,
        },
        // 3.3 Empty Country - Feature Explicitly Disabled.
        PrivacySandboxConfirmationTestData{
            .disabled_features = {kPrivacySandboxSettings4},
            .variation_country = "",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_false = true,
        },
        // 3.4 Empty Country - Feature Not Set.
        PrivacySandboxConfirmationTestData{
            .variation_country = "",
            // Expectations
            .expect_required = false,
        }));

class PrivacySandboxRestrictedNoticeConfirmationTest
    : public PrivacySandboxConfirmationTestBase {};

base::test::FeatureRefAndParams RestrictedNoticeFeature() {
  return {kPrivacySandboxSettings4,
          {{kPrivacySandboxSettings4RestrictedNoticeName, "true"}}};
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxRestrictedNoticeConfirmationTest,
                       RestrictedNoticeTest) {
  // Setup
  base::HistogramTester histogram_tester;
  g_browser_process->variations_service()->OverrideStoredPermanentCountry(
      GetParam().variation_country);

  EXPECT_EQ(IsRestrictedNoticeRequired(), GetParam().expect_required);
  histogram_tester.ExpectBucketCount(
      "Settings.PrivacySandbox.RestrictedNoticeCheckIsMismatched", true,
      GetParam().expect_mismatch_histogram_true);
  histogram_tester.ExpectBucketCount(
      "Settings.PrivacySandbox.RestrictedNoticeCheckIsMismatched", false,
      GetParam().expect_mismatch_histogram_false);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrivacySandboxRestrictedNoticeConfirmationTest,
    testing::Values(
        // Consent Required, Feature Not Overridden.
        PrivacySandboxConfirmationTestData{
            .variation_country = "gb",
            // Expectations
            .expect_required = true,
        },
        // Notice Required, Feature Not Overridden.
        PrivacySandboxConfirmationTestData{
            .variation_country = "us",
            // Expectations
            .expect_required = true,
        },
        // Consent Not required. Notice Not required - Feature Overridden.
        // restricted-notice param not set.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {{kPrivacySandboxSettings4, {{}}}},
            .variation_country = "",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_false = true,
        },
        // Consent Not required. Notice Not required - Feature Overridden.
        // restricted-notice param set.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {RestrictedNoticeFeature()},
            .variation_country = "",
            // Expectations
            .expect_required = true,
            .expect_mismatch_histogram_true = true,
        },
        // Notice required - Feature Overridden. restricted-notice param set.
        PrivacySandboxConfirmationTestData{
            .enabled_features =
                {{kPrivacySandboxSettings4,
                  {{kPrivacySandboxSettings4NoticeRequiredName, "true"},
                   {kPrivacySandboxSettings4RestrictedNoticeName, "true"}}}},
            .variation_country = "",
            // Expectations
            .expect_required = true,
            .expect_mismatch_histogram_false = true,
        },
        // Consent required - Feature Overridden. restricted-notice param set.
        PrivacySandboxConfirmationTestData{
            .enabled_features =
                {{kPrivacySandboxSettings4,
                  {{kPrivacySandboxSettings4ConsentRequiredName, "true"},
                   {kPrivacySandboxSettings4RestrictedNoticeName, "true"}}}},
            .variation_country = "",
            // Expectations
            .expect_required = true,
            .expect_mismatch_histogram_false = true,
        },
        // Notice required - Feature Overridden. restricted-notice param Not
        // set.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {NoticeFeature()},
            .variation_country = "",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_true = true,
        },
        // Consent required - Feature Overridden. restricted-notice param Not
        // set.
        PrivacySandboxConfirmationTestData{
            .enabled_features = {ConsentFeature()},
            .variation_country = "",
            // Expectations
            .expect_required = false,
            .expect_mismatch_histogram_true = true,
        }));

}  // namespace
}  // namespace privacy_sandbox

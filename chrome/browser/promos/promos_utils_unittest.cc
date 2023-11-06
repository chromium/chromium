// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/promos/promos_utils.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/promos/promos_features.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace promos_utils {

class IOSPasswordPromoOnDesktopTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Register the prefs when not on a branded build (they're automatically
    // registered when on a branded build).
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    promos_utils::RegisterProfilePrefs(prefs()->registry());
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  }

  // Getter for the test syncable prefs service.
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }

  // Getter for the browser task environment.
  content::BrowserTaskEnvironment* environment() { return &task_environment_; }

  // Getter for the histograms tester.
  base::HistogramTester* histograms() { return &histogram_; }

  // Getter for the features list.
  base::test::ScopedFeatureList* features() { return &scoped_feature_list_; }

  // Getter for the testing profile.
  TestingProfile* profile() { return &profile_; }

  // Enables the iOS Password promo feature with a "contextual-direct" param.
  void EnableContextualDirectFeature() {
    features()->InitWithFeaturesAndParameters(
        {{promos_features::kIOSPromoPasswordBubble,
          {{"activation", "contextual-direct"}}}},
        {/* disabled_features */});
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::HistogramTester histogram_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
};

// Tests that RegisterProfilePrefs registers the prefs to their default values
// correctly.
TEST_F(IOSPasswordPromoOnDesktopTest, RegisterProfilePrefsTest) {
  ASSERT_FALSE(prefs()->GetBoolean(promos_prefs::kiOSPasswordPromoOptOut));
  ASSERT_EQ(
      prefs()->GetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter),
      0);
  ASSERT_EQ(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      base::Time());
}

// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for first impression and action dismissed.
TEST_F(
    IOSPasswordPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestFirstImpressionDismissed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      1, DesktopIOSPasswordPromoAction::kDismissed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.FirstImpression.Action",
      DesktopIOSPasswordPromoAction::kDismissed, 1);
}

// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for first impression and action explicitly closed.
TEST_F(
    IOSPasswordPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestFirstImpressionClosed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      1, DesktopIOSPasswordPromoAction::kExplicitlyClosed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.FirstImpression.Action",
      DesktopIOSPasswordPromoAction::kExplicitlyClosed, 1);
}

// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for first impression and action get started button clicked.
TEST_F(
    IOSPasswordPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestFirstImpressionGetStartedClicked) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      1, DesktopIOSPasswordPromoAction::kGetStartedClicked);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.FirstImpression.Action",
      DesktopIOSPasswordPromoAction::kGetStartedClicked, 1);
}

// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for second impression and action dismissed.
TEST_F(
    IOSPasswordPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestSecondImpressionDismissed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      2, DesktopIOSPasswordPromoAction::kDismissed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.SecondImpression.Action",
      DesktopIOSPasswordPromoAction::kDismissed, 1);
}

// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for second impression and action explicitly closed.
TEST_F(
    IOSPasswordPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestSecondImpressionClosed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      2, DesktopIOSPasswordPromoAction::kExplicitlyClosed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.SecondImpression.Action",
      DesktopIOSPasswordPromoAction::kExplicitlyClosed, 1);
}

// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for second impression and action get started button clicked.
TEST_F(
    IOSPasswordPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestSecondImpressionGetStartedClicked) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      2, DesktopIOSPasswordPromoAction::kGetStartedClicked);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.SecondImpression.Action",
      DesktopIOSPasswordPromoAction::kGetStartedClicked, 1);
}

// Tests that IsActivationCriteriaOverriddenIOSPasswordPromo returns true when
// the feature flag is set to override criteria.
TEST_F(IOSPasswordPromoOnDesktopTest,
       IsActivationCriteriaOverriddenIOSPasswordPromoTrueTest) {
  features()->InitWithFeaturesAndParameters(
      {{promos_features::kIOSPromoPasswordBubble,
        {{"activation", "always-show-indirect"}}}},
      {/* disabled_features */});
  EXPECT_TRUE(promos_utils::IsActivationCriteriaOverriddenIOSPasswordPromo());
}

// Tests that IsActivationCriteriaOverriddenIOSPasswordPromo returns false when
// the feature flag is set to not override criteria.
TEST_F(IOSPasswordPromoOnDesktopTest,
       IsActivationCriteriaOverriddenIOSPasswordPromoFalseTest) {
  EnableContextualDirectFeature();
  EXPECT_FALSE(promos_utils::IsActivationCriteriaOverriddenIOSPasswordPromo());
}

// Tests that ShouldShowIOSPasswordPromo returns true when no promo has yet been
// shown and the feature flag is set.
TEST_F(IOSPasswordPromoOnDesktopTest, ShouldShowIOSPasswordPromoTestTrue) {
  EnableContextualDirectFeature();
  EXPECT_TRUE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// Tests that ShouldShowIOSPasswordPromo returns false when the feature flag is
// not properly set.
TEST_F(IOSPasswordPromoOnDesktopTest,
       ShouldShowIOSPasswordPromoTestFalseWrongFeatureFlag) {
  features()->InitWithFeaturesAndParameters(
      {{promos_features::kIOSPromoPasswordBubble,
        {{"activation", "always-show-indirect"}}}},
      {/* disabled_features */});
  EXPECT_FALSE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// Tests that ShouldShowIOSPasswordPromo returns false when the user has already
// seen 2 promos.
TEST_F(IOSPasswordPromoOnDesktopTest,
       ShouldShowIOSPasswordPromoTestFalseTooManyImpressions) {
  EnableContextualDirectFeature();
  prefs()->SetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter, 2);
  EXPECT_FALSE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// Tests that ShouldShowIOSPasswordPromo returns false when the last seen
// impression is too recent.
TEST_F(IOSPasswordPromoOnDesktopTest,
       ShouldShowIOSPasswordPromoTestFalseLastImpressionTooRecent) {
  EnableContextualDirectFeature();
  prefs()->SetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp,
                   base::Time::Now());
  EXPECT_FALSE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// Tests that ShouldShowIOSPasswordPromo returns false when the user has
// opted-out from the promo.
TEST_F(IOSPasswordPromoOnDesktopTest,
       ShouldShowIOSPasswordPromoTestFalseUserOptedOut) {
  EnableContextualDirectFeature();
  prefs()->SetBoolean(promos_prefs::kiOSPasswordPromoOptOut, true);
  EXPECT_FALSE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// Tests that UserNotClassifiedAsMobileDeviceSwitcher returns true when the
// result is successful and the mobile labels are not present in the
// classification labels.
TEST_F(IOSPasswordPromoOnDesktopTest,
       UserNotClassifiedAsMobileDeviceSwitcherTestTrue) {
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back("test_label");
  EXPECT_TRUE(promos_utils::UserNotClassifiedAsMobileDeviceSwitcher(result));
}

// Tests that UserNotClassifiedAsMobileDeviceSwitcher returns false when the
// result has an error.
TEST_F(IOSPasswordPromoOnDesktopTest,
       UserNotClassifiedAsMobileDeviceSwitcherTestFalseError) {
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kFailed);
  EXPECT_FALSE(promos_utils::UserNotClassifiedAsMobileDeviceSwitcher(result));
}

// Tests that UserNotClassifiedAsMobileDeviceSwitcher returns false when a
// mobile label is present in the classification results.
TEST_F(IOSPasswordPromoOnDesktopTest,
       UserNotClassifiedAsMobileDeviceSwitcherTestFalseMobileLabelPresent) {
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back(
      segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel);
  EXPECT_FALSE(promos_utils::UserNotClassifiedAsMobileDeviceSwitcher(result));
}

// Tests that iOSPasswordPromoShown sets the correct prefs and records the
// correct histogram for the first impression.
TEST_F(IOSPasswordPromoOnDesktopTest,
       iOSPasswordPromoShownTestFirstImpression) {
  // Record before and after times to ensure the timestamp is within that range.
  base::Time before = base::Time::Now();
  promos_utils::iOSPasswordPromoShown(profile());
  base::Time after = base::Time::Now();

  ASSERT_EQ(
      prefs()->GetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter),
      1);
  ASSERT_GE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      before);
  ASSERT_LE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      after);

  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.Shown",
      promos_utils::DesktopIOSPasswordPromoImpression::kFirstImpression, 1);
}

// Tests that iOSPasswordPromoShown sets the correct prefs and records the
// correct histogram for the second impression.
TEST_F(IOSPasswordPromoOnDesktopTest,
       iOSPasswordPromoShownTestSecondImpression) {
  // First impression
  promos_utils::iOSPasswordPromoShown(profile());

  // Second impression
  base::Time before = base::Time::Now();
  promos_utils::iOSPasswordPromoShown(profile());
  base::Time after = base::Time::Now();

  ASSERT_EQ(
      prefs()->GetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter),
      2);
  ASSERT_GE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      before);
  ASSERT_LE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      after);

  histograms()->ExpectBucketCount(
      "IOS.DesktopPasswordPromo.Shown",
      promos_utils::DesktopIOSPasswordPromoImpression::kSecondImpression, 1);
}

// Tests that IsDirectVariantIOSPasswordPromo returns true when the user is in a
// direct variant of the feature flag.
TEST_F(IOSPasswordPromoOnDesktopTest, IsDirectVariantIOSPasswordPromoTestTrue) {
  EnableContextualDirectFeature();
  EXPECT_TRUE(promos_utils::IsDirectVariantIOSPasswordPromo());
}

// Tests that IsDirectVariantIOSPasswordPromo returns true since feature is
// enabled by default.
TEST_F(IOSPasswordPromoOnDesktopTest,
       IsDirectVariantIOSPasswordPromoTestFalseFeatureInactive) {
  EXPECT_TRUE(promos_utils::IsDirectVariantIOSPasswordPromo());
}

// Tests that IsDirectVariantIOSPasswordPromo returns false when the user's
// feature is set to an indirect variant.
TEST_F(IOSPasswordPromoOnDesktopTest,
       IsDirectVariantIOSPasswordPromoTestFalseIndirectActive) {
  features()->InitWithFeaturesAndParameters(
      {{promos_features::kIOSPromoPasswordBubble,
        {{"activation", "always-show-indirect"}}}},
      {/* disabled_features */});
  EXPECT_FALSE(promos_utils::IsDirectVariantIOSPasswordPromo());
}

// Tests that IsIndirectVariantIOSPasswordPromo returns true when the user is in
// an indirect variant of the feature flag.
TEST_F(IOSPasswordPromoOnDesktopTest,
       IsIndirectVariantIOSPasswordPromoTestTrue) {
  features()->InitWithFeaturesAndParameters(
      {{promos_features::kIOSPromoPasswordBubble,
        {{"activation", "always-show-indirect"}}}},
      {/* disabled_features */});
  EXPECT_TRUE(promos_utils::IsIndirectVariantIOSPasswordPromo());
}

// Tests that IsIndirectVariantIOSPasswordPromo returns false when the feature
// is not active.
TEST_F(IOSPasswordPromoOnDesktopTest,
       IsIndirectVariantIOSPasswordPromoTestFalseFeatureInactive) {
  EXPECT_FALSE(promos_utils::IsIndirectVariantIOSPasswordPromo());
}

// Tests that IsIndirectVariantIOSPasswordPromo returns false when the user's
// feature is set to a direct variant.
TEST_F(IOSPasswordPromoOnDesktopTest,
       IsIndirectVariantIOSPasswordPromoTestFalseDirectActive) {
  EnableContextualDirectFeature();
  EXPECT_FALSE(promos_utils::IsIndirectVariantIOSPasswordPromo());
}
}  // namespace promos_utils

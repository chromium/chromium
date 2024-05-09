// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/promos/promos_utils.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/promos/promos_types.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace promos_utils {

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
class IOSPromoOnDesktopTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Register the prefs when not on a branded build (they're automatically
    // registered when on a branded build).
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    RegisterProfilePrefs(prefs()->registry());
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

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::HistogramTester histogram_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
};

// Tests that RecordIOSPromoUserInteractionHistogram records the proper
// histogram for first impression and action dismissed for the password promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       RecordIOSPromoUserInteractionHistogramTestFirstImpressionDismissed) {
  RecordIOSPromoUserInteractionHistogram(IOSPromoType::kPassword, 1,
                                         DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSPromoUserInteractionHistogram records the proper
// histogram for first impression and no thanks clicked action for the password
// promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSPromoUserInteractionHistogramTestFirstImpressionNoThanksClicked) {
  RecordIOSPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 1, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSPromoUserInteractionHistogram records the proper
// histogram for second impression and action dismissed for the password promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       RecordIOSPromoUserInteractionHistogramTestSecondImpressionDismissed) {
  RecordIOSPromoUserInteractionHistogram(IOSPromoType::kPassword, 2,
                                         DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSPromoUserInteractionHistogram records the proper
// histogram for second impression and no thanks clicked action for the
// password promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSPromoUserInteractionHistogramTestSecondImpressionNoThanksClicked) {
  RecordIOSPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 2, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPasswordPromoUserInteractionHistogram records the
// proper histogram for first impression and action dismissed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPasswordPromoUserInteractionHistogramTestFirstImpressionDismissed) {
  RecordIOSDesktopPasswordPromoUserInteractionHistogram(
      1, DesktopIOSPromoAction::kDismissed);
  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPasswordPromoUserInteractionHistogram records the
// proper histogram for first impression and action explicitly closed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPasswordPromoUserInteractionHistogramTestFirstImpressionNoThanksClicked) {
  RecordIOSDesktopPasswordPromoUserInteractionHistogram(
      1, DesktopIOSPromoAction::kNoThanksClicked);
  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPasswordPromoUserInteractionHistogram records the
// proper histogram for second impression and action dismissed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPasswordPromoUserInteractionHistogramTestSecondImpressionDismissed) {
  RecordIOSDesktopPasswordPromoUserInteractionHistogram(
      2, DesktopIOSPromoAction::kDismissed);
  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPasswordPromoUserInteractionHistogram records the
// proper histogram for second impression and action explicitly closed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPasswordPromoUserInteractionHistogramTestSecondImpressionNoThanksClicked) {
  RecordIOSDesktopPasswordPromoUserInteractionHistogram(
      2, DesktopIOSPromoAction::kNoThanksClicked);
  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that ShouldShowIOSDesktopPromo returns true when no promo has yet been
// shown for the given password promo type.
TEST_F(IOSPromoOnDesktopTest, ShouldShowIOSDesktopPromoTestTrue) {
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has already
// seen 2 promos for the given password promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseTooManyImpressions) {
  prefs()->SetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter, 2);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the last seen
// impression is too recent for the given password promo type..
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseLastImpressionTooRecent) {
  prefs()->SetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp,
                   base::Time::Now());
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has
// opted-out from the given password promo type.
TEST_F(IOSPromoOnDesktopTest, ShouldShowIOSDesktopPromoTestFalseUserOptedOut) {
  prefs()->SetBoolean(promos_prefs::kiOSPasswordPromoOptOut, true);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kPassword));
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the first impression for the given password promo
// type.
TEST_F(IOSPromoOnDesktopTest, IOSDesktopPromoShownTestFirstImpression) {
  // Record before and after times to ensure the timestamp is within that range.
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);
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
      "IOS.Desktop.PasswordPromo.Shown",
      DesktopIOSPasswordPromoImpression::kFirstImpression, 1);
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the second impression for the given password promo
// type.
TEST_F(IOSPromoOnDesktopTest, IOSDesktopPromoShownTestSecondImpression) {
  // First impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);

  // Second impression
  base::Time before = base::Time::Now();
  IOSDesktopPasswordPromoShown(profile());
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

  histograms()->ExpectBucketCount("IOS.Desktop.PasswordPromo.Shown",
                                  DesktopIOSPromoImpression::kSecondImpression,
                                  1);
}

// Tests that ShouldShowIOSDesktopPasswordPromo returns true when no promo has
// yet been shown.
TEST_F(IOSPromoOnDesktopTest, ShouldShowIOSDesktopPasswordPromoTestTrue) {
  EXPECT_TRUE(ShouldShowIOSDesktopPasswordPromo(profile()));
}

// Tests that ShouldShowIOSDesktopPasswordPromo returns false when the user has
// already seen 2 promos.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPasswordPromoTestFalseTooManyImpressions) {
  prefs()->SetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter, 2);
  EXPECT_FALSE(ShouldShowIOSDesktopPasswordPromo(profile()));
}

// Tests that ShouldShowIOSDesktopPasswordPromo returns false when the last seen
// impression is too recent.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPasswordPromoTestFalseLastImpressionTooRecent) {
  prefs()->SetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp,
                   base::Time::Now());
  EXPECT_FALSE(ShouldShowIOSDesktopPasswordPromo(profile()));
}

// Tests that ShouldShowIOSDesktopPasswordPromo returns false when the user has
// opted-out from the promo.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPasswordPromoTestFalseUserOptedOut) {
  prefs()->SetBoolean(promos_prefs::kiOSPasswordPromoOptOut, true);
  EXPECT_FALSE(ShouldShowIOSDesktopPasswordPromo(profile()));
}

// Tests that IOSDesktopPasswordPromoShown sets the correct prefs and records
// the correct histogram for the first impression.
TEST_F(IOSPromoOnDesktopTest, IOSDesktopPasswordPromoShownTestFirstImpression) {
  // Record before and after times to ensure the timestamp is within that range.
  base::Time before = base::Time::Now();
  IOSDesktopPasswordPromoShown(profile());
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

  histograms()->ExpectUniqueSample("IOS.Desktop.PasswordPromo.Shown",
                                   DesktopIOSPromoImpression::kFirstImpression,
                                   1);
}

// Tests that IOSDesktopPasswordPromoShown sets the correct prefs and records
// the correct histogram for the second impression.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPasswordPromoShownTestSecondImpression) {
  // First impression
  IOSDesktopPasswordPromoShown(profile());

  // Second impression
  base::Time before = base::Time::Now();
  IOSDesktopPasswordPromoShown(profile());
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

  histograms()->ExpectBucketCount("IOS.Desktop.PasswordPromo.Shown",
                                  DesktopIOSPromoImpression::kSecondImpression,
                                  1);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for first impression and action dismissed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestFirstImpressionDismissed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      1, DesktopIOSPasswordPromoAction::kDismissed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.FirstImpression.Action",
      DesktopIOSPasswordPromoAction::kDismissed, 1);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for first impression and action explicitly closed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestFirstImpressionClosed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      1, DesktopIOSPasswordPromoAction::kExplicitlyClosed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.FirstImpression.Action",
      DesktopIOSPasswordPromoAction::kExplicitlyClosed, 1);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for second impression and action dismissed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestSecondImpressionDismissed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      2, DesktopIOSPasswordPromoAction::kDismissed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.SecondImpression.Action",
      DesktopIOSPasswordPromoAction::kDismissed, 1);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for second impression and action explicitly closed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestSecondImpressionClosed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      2, DesktopIOSPasswordPromoAction::kExplicitlyClosed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.SecondImpression.Action",
      DesktopIOSPasswordPromoAction::kExplicitlyClosed, 1);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that ShouldShowIOSPasswordPromo returns true when no promo has yet been
// shown.
TEST_F(IOSPromoOnDesktopTest, ShouldShowIOSPasswordPromoTestTrue) {
  EXPECT_TRUE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that ShouldShowIOSPasswordPromo returns false when the user has already
// seen 2 promos.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSPasswordPromoTestFalseTooManyImpressions) {
  prefs()->SetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter, 2);
  EXPECT_FALSE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that ShouldShowIOSPasswordPromo returns false when the last seen
// impression is too recent.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSPasswordPromoTestFalseLastImpressionTooRecent) {
  prefs()->SetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp,
                   base::Time::Now());
  EXPECT_FALSE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that ShouldShowIOSPasswordPromo returns false when the user has
// opted-out from the promo.
TEST_F(IOSPromoOnDesktopTest, ShouldShowIOSPasswordPromoTestFalseUserOptedOut) {
  prefs()->SetBoolean(promos_prefs::kiOSPasswordPromoOptOut, true);
  EXPECT_FALSE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// Tests that UserNotClassifiedAsMobileDeviceSwitcher returns true when the
// result is successful and the mobile labels are not present in the
// classification labels.
TEST_F(IOSPromoOnDesktopTest, UserNotClassifiedAsMobileDeviceSwitcherTestTrue) {
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back("test_label");
  EXPECT_TRUE(promos_utils::UserNotClassifiedAsMobileDeviceSwitcher(result));
}

// Tests that UserNotClassifiedAsMobileDeviceSwitcher returns false when the
// result has an error.
TEST_F(IOSPromoOnDesktopTest,
       UserNotClassifiedAsMobileDeviceSwitcherTestFalseError) {
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kFailed);
  EXPECT_FALSE(promos_utils::UserNotClassifiedAsMobileDeviceSwitcher(result));
}

// Tests that UserNotClassifiedAsMobileDeviceSwitcher returns false when a
// mobile label is present in the classification results.
TEST_F(IOSPromoOnDesktopTest,
       UserNotClassifiedAsMobileDeviceSwitcherTestFalseMobileLabelPresent) {
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back(
      segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel);
  EXPECT_FALSE(promos_utils::UserNotClassifiedAsMobileDeviceSwitcher(result));
}

// Tests that RegisterProfilePrefs registers the prefs to their default values
// correctly.
TEST_F(IOSPromoOnDesktopTest, RegisterProfilePrefsTest) {
  ASSERT_FALSE(prefs()->GetBoolean(promos_prefs::kiOSPasswordPromoOptOut));
  ASSERT_EQ(
      prefs()->GetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter),
      0);
  ASSERT_EQ(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      base::Time());
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that iOSPasswordPromoShown sets the correct prefs and records the
// correct histogram for the first impression.
TEST_F(IOSPromoOnDesktopTest, iOSPasswordPromoShownTestFirstImpression) {
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

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that iOSPasswordPromoShown sets the correct prefs and records the
// correct histogram for the second impression.
TEST_F(IOSPromoOnDesktopTest, iOSPasswordPromoShownTestSecondImpression) {
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
}  // namespace promos_utils

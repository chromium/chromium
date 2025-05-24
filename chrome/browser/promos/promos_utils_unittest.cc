// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/promos/promos_utils.h"

#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/promos/promos_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace promos_utils {

class IOSPromoOnDesktopTest : public ::testing::Test {
 public:
  void SetUp() override {
    sync_service_.GetUserSettings()->SetSelectedTypes(/*sync_everything=*/true,
                                                      {});
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

  // Getter for the testing sync_service.
  syncer::TestSyncService* sync_service() { return &sync_service_; }

 protected:
  ScopedTestingLocalState scoped_testing_local_state_{
      TestingBrowserProcess::GetGlobal()};

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::HistogramTester histogram_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
  syncer::TestSyncService sync_service_;
};

// Tests RecordIOSDesktopPromoUserInteractionHistogram for all promo types.
// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for first impression and action dismissed for the password promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestFirstImpressionDismissedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 1, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for first impression and no thanks clicked action for the password
// promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestFirstImpressionNoThanksClickedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 1, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for second impression and action dismissed for the password promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestSecondImpressionDismissedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 2, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for second impression and no thanks clicked action for the password
// promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestSecondImpressionNoThanksClickedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 2, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for third impression and action dismissed for the password promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestThirdImpressionDismissedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 3, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.ThirdImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for third impression and no thanks clicked action for the
// password promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestThirdImpressionNoThanksClickedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 3, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.ThirdImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for first impression and action dismissed for the address promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestFirstImpressionDismissedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 1, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for first impression and no thanks clicked action for the address
// promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestFirstImpressionNoThanksClickedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 1, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for second impression and action dismissed for the address promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestSecondImpressionDismissedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 2, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for second impression and no thanks clicked action for the
// address promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestSecondImpressionNoThanksClickedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 2, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for third impression and action dismissed for the address promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestThirdImpressionDismissedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 3, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.ThirdImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for third impression and no thanks clicked action for the
// address promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestThirdImpressionNoThanksClickedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 3, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.ThirdImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests ShouldShowIOSDesktopPromo with all promo types.
// Tests that ShouldShowIOSDesktopPromo returns true when no promo has yet been
// shown for the given password promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestTrueForPasswordPromo) {
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                        IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the promotions are
// disabled.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalsePromotionsDisabled) {
  scoped_testing_local_state_.Get()->SetBoolean(prefs::kPromotionsEnabled,
                                                false);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has already
// seen 3 promos for the given password promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseTooManyImpressionsForPasswordPromo) {
  prefs()->SetInteger(
      promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter, 3);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the last seen
// impression is too recent for the given password promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    ShouldShowIOSDesktopPromoTestFalseLastImpressionTooRecentForPasswordPromo) {
  prefs()->SetTime(
      promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp,
      base::Time::Now());
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has
// opted-out from the given password promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseUserOptedOutForPasswordPromo) {
  prefs()->SetBoolean(promos_prefs::kDesktopToiOSPasswordPromoOptOut, true);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns true when no promo has yet been
// shown for the given address promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestTrueForAddressPromo) {
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                        IOSPromoType::kAddress));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has already
// seen 3 promos for the given address promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseTooManyImpressionsForAddressPromo) {
  prefs()->SetInteger(promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter,
                      3);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kAddress));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the last seen
// impression is too recent for the given address promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    ShouldShowIOSDesktopPromoTestFalseLastImpressionTooRecentForAddressPromo) {
  prefs()->SetTime(
      promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp,
      base::Time::Now());
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kAddress));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has
// opted-out from the given address promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseUserOptedOutForAddressPromo) {
  prefs()->SetBoolean(promos_prefs::kDesktopToiOSAddressPromoOptOut, true);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kAddress));
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the first impression for the given password promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestFirstImpressionForPasswordPromo) {
  // Record before and after times to ensure the timestamp is within that range.
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter),
            1);
  ASSERT_GE(
      prefs()->GetTime(
          promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp),
      before);
  ASSERT_LE(
      prefs()->GetTime(
          promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp),
      after);

  histograms()->ExpectUniqueSample("IOS.Desktop.PasswordPromo.Shown",
                                   DesktopIOSPromoImpression::kFirstImpression,
                                   1);
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the second impression for the given password promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestSecondImpressionForPasswordPromo) {
  // First impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);

  // Second impression
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter),
            2);
  ASSERT_GE(
      prefs()->GetTime(
          promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp),
      before);
  ASSERT_LE(
      prefs()->GetTime(
          promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp),
      after);

  histograms()->ExpectBucketCount("IOS.Desktop.PasswordPromo.Shown",
                                  DesktopIOSPromoImpression::kSecondImpression,
                                  1);
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the third impression for the given password promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestThirdImpressionForPasswordPromo) {
  // First impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);

  // Second impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);

  // Third impression
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter),
            3);
  ASSERT_GE(
      prefs()->GetTime(
          promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp),
      before);
  ASSERT_LE(
      prefs()->GetTime(
          promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp),
      after);

  histograms()->ExpectBucketCount("IOS.Desktop.PasswordPromo.Shown",
                                  DesktopIOSPromoImpression::kThirdImpression,
                                  1);
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the first impression for the given address promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestFirstImpressionForAddressPromo) {
  // Record before and after times to ensure the timestamp is within that range.
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter),
            1);
  ASSERT_GE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            before);
  ASSERT_LE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            after);

  histograms()->ExpectUniqueSample("IOS.Desktop.AddressPromo.Shown",
                                   DesktopIOSPromoImpression::kFirstImpression,
                                   1);
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the second impression for the given address promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestSecondImpressionForAddressPromo) {
  // First impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);

  // Second impression
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter),
            2);
  ASSERT_GE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            before);
  ASSERT_LE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            after);

  histograms()->ExpectBucketCount("IOS.Desktop.AddressPromo.Shown",
                                  DesktopIOSPromoImpression::kSecondImpression,
                                  1);
}
// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the third impression for the given address promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestThirdImpressionForAddressPromo) {
  // First impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);

  // Second impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);

  // Third impression
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter),
            3);
  ASSERT_GE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            before);
  ASSERT_LE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            after);

  histograms()->ExpectBucketCount("IOS.Desktop.AddressPromo.Shown",
                                  DesktopIOSPromoImpression::kThirdImpression,
                                  1);
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
  // Password promo.
  ASSERT_FALSE(
      prefs()->GetBoolean(promos_prefs::kDesktopToiOSPasswordPromoOptOut));
  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter),
            0);
  ASSERT_EQ(
      prefs()->GetTime(
          promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp),
      base::Time());

  // Address promo.
  ASSERT_FALSE(
      prefs()->GetBoolean(promos_prefs::kDesktopToiOSAddressPromoOptOut));
  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter),
            0);
  ASSERT_EQ(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            base::Time());

  // Payment promo.
  ASSERT_FALSE(
      prefs()->GetBoolean(promos_prefs::kDesktopToiOSPaymentPromoOptOut));
  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter),
            0);
  ASSERT_EQ(prefs()->GetTime(
                promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp),
            base::Time());
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for first impression and action dismissed for the payment promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestFirstImpressionDismissedForPaymentPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPayment, 1, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PaymentPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for first impression and no thanks clicked action for the payment
// promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestFirstImpressionNoThanksClickedForPaymentPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPayment, 1, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PaymentPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for second impression and action dismissed for the payment promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestSecondImpressionDismissedForPaymentPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPayment, 2, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PaymentPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for second impression and no thanks clicked action for the
// payment promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestSecondImpressionNoThanksClickedForPaymentPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPayment, 2, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PaymentPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for third impression and action dismissed for the payment promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestThirdImpressionDismissedForPaymentPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPayment, 3, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PaymentPromo.ThirdImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for third impression and no thanks clicked action for the
// payment promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestThirdImpressionNoThanksClickedForPaymentPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPayment, 3, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PaymentPromo.ThirdImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that ShouldShowIOSDesktopPromo returns true when no promo has yet been
// shown for the given payment promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestTrueForPaymentPromo) {
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                        IOSPromoType::kPayment));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has already
// seen 3 promos for the given payment promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseTooManyImpressionsForPaymentPromo) {
  prefs()->SetInteger(promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter,
                      3);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kPayment));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the last seen
// impression is too recent for the given payment promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    ShouldShowIOSDesktopPromoTestFalseLastImpressionTooRecentForPaymentPromo) {
  prefs()->SetTime(
      promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp,
      base::Time::Now());
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kPayment));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has
// opted-out from the given payment promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseUserOptedOutForPaymentPromo) {
  prefs()->SetBoolean(promos_prefs::kDesktopToiOSPaymentPromoOptOut, true);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kPayment));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has
// seen too many other promos and that the Desktop NTP promo only counts as 1,
// no matter how many times it has actually shown.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseTooManyOtherPromos) {
  // First, make sure that 10 is the limit.
  prefs()->SetInteger(promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter,
                      10);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kAddress));

  prefs()->SetInteger(promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter,
                      9);
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                        IOSPromoType::kAddress));

  // Go two below the limit so adding Desktop NTP promo impressions pushes back
  // to one below the limit.
  prefs()->SetInteger(promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter,
                      8);
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                        IOSPromoType::kAddress));

  base::Time promo_time = base::Time::Now() - base::Days(1000);
  base::Value::List desktop_ntp_promo_timestamps;
  desktop_ntp_promo_timestamps.Append(base::TimeToValue(promo_time));
  prefs()->SetList(promos_prefs::kDesktopToiOSNtpPromoAppearanceTimestamps,
                   std::move(desktop_ntp_promo_timestamps));
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                        IOSPromoType::kAddress));

  // Add a second timestamp and the promo should still be able to be shown.
  {
    ScopedListPrefUpdate update(
        prefs(), promos_prefs::kDesktopToiOSNtpPromoAppearanceTimestamps);
    update->Append(base::TimeToValue(promo_time + base::Seconds(1)));
  }
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                        IOSPromoType::kAddress));

  // Setting another promo's count higher should block the promo again.
  prefs()->SetInteger(promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter,
                      9);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kAddress));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has
// seen the Desktop Ntp promo too recently.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseDesktopNtpPromoTooRecent) {
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                        IOSPromoType::kAddress));

  prefs()->SetList(
      promos_prefs::kDesktopToiOSNtpPromoAppearanceTimestamps,
      base::Value::List().Append(base::TimeToValue(base::Time::Now())));
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kAddress));
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the first impression for the given payment promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestFirstImpressionForPaymentPromo) {
  // Record before and after times to ensure the timestamp is within that range.
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kPayment);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter),
            1);
  ASSERT_GE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp),
            before);
  ASSERT_LE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp),
            after);

  histograms()->ExpectUniqueSample("IOS.Desktop.PaymentPromo.Shown",
                                   DesktopIOSPromoImpression::kFirstImpression,
                                   1);
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the second impression for the given payment promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestSecondImpressionForPaymentPromo) {
  // First impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kPayment);

  // Second impression
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kPayment);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter),
            2);
  ASSERT_GE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp),
            before);
  ASSERT_LE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp),
            after);

  histograms()->ExpectBucketCount("IOS.Desktop.PaymentPromo.Shown",
                                  DesktopIOSPromoImpression::kSecondImpression,
                                  1);
}
// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the third impression for the given payment promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestThirdImpressionForPaymentPromo) {
  // First impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kPayment);

  // Second impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kPayment);

  // Third impression
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kPayment);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter),
            3);
  ASSERT_GE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp),
            before);
  ASSERT_LE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp),
            after);

  histograms()->ExpectBucketCount("IOS.Desktop.PaymentPromo.Shown",
                                  DesktopIOSPromoImpression::kThirdImpression,
                                  1);
}

// Tests getting the correct password promo Feature Engagement Tracker
// feature.
TEST_F(IOSPromoOnDesktopTest, GetIOSDesktopPromoFeatureEngagementPasswords) {
  const base::Feature& feature =
      GetIOSDesktopPromoFeatureEngagement(IOSPromoType::kPassword);

  ASSERT_EQ(&feature, &feature_engagement::kIPHiOSPasswordPromoDesktopFeature);
}

// Tests getting the correct address promo Feature Engagement Tracker feature.
TEST_F(IOSPromoOnDesktopTest, GetIOSDesktopPromoFeatureEngagementAddress) {
  const base::Feature& feature =
      GetIOSDesktopPromoFeatureEngagement(IOSPromoType::kAddress);

  ASSERT_EQ(&feature, &feature_engagement::kIPHiOSAddressPromoDesktopFeature);
}

// Tests that ShouldShowIOSDesktopPromo returns true when the correct datatypes
// are syncing.
TEST_F(IOSPromoOnDesktopTest,
       PasswordPromoSyncPrefsPasswordsAndPreferencesEnabled) {
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/
      {syncer::UserSelectableType::kPasswords,
       syncer::UserSelectableType::kPreferences});
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                        IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the preferences
// datatype is not syncing.
TEST_F(IOSPromoOnDesktopTest,
       PasswordPromoSyncPrefsPasswordsEnabledPreferencesDisabled) {
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/
      {syncer::UserSelectableType::kPasswords});
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the passwords
// datatype is not syncing.
TEST_F(IOSPromoOnDesktopTest,
       PasswordPromoSyncPrefsPreferencesEnabledPasswordsDisabled) {
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/
      {syncer::UserSelectableType::kPreferences});
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns true when the correct datatypes
// are syncing.
TEST_F(IOSPromoOnDesktopTest,
       AddressPromoSyncPrefsAutofillAndPreferencesEnabled) {
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/
      {syncer::UserSelectableType::kAutofill,
       syncer::UserSelectableType::kPreferences});
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                        IOSPromoType::kAddress));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the preferences
// datatype is not syncing.
TEST_F(IOSPromoOnDesktopTest,
       AddressPromoSyncPrefsAutofillEnabledPreferencesDisabled) {
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/
      {syncer::UserSelectableType::kAutofill});
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kAddress));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the autofill datatype
// is not syncing.
TEST_F(IOSPromoOnDesktopTest,
       AddressPromoSyncPrefsPreferencesEnabledAutofillDisabled) {
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/
      {syncer::UserSelectableType::kPreferences});
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kAddress));
}

// Tests that ShouldShowIOSDesktopPromo returns true when the correct datatypes
// are syncing.
TEST_F(IOSPromoOnDesktopTest,
       PaymentPromoSyncPrefsPaymentsAndPreferencesEnabled) {
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/
      {syncer::UserSelectableType::kPayments,
       syncer::UserSelectableType::kPreferences});
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                        IOSPromoType::kPayment));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the preferences
// datatype is not syncing.
TEST_F(IOSPromoOnDesktopTest,
       PaymentPromoSyncPrefsPaymentsEnabledPreferencesDisabled) {
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/
      {syncer::UserSelectableType::kPayments});
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kPayment));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the payments datatype
// is not syncing.
TEST_F(IOSPromoOnDesktopTest,
       PaymentPromoSyncPrefsPreferencesEnabledPaymentsDisabled) {
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/
      {syncer::UserSelectableType::kPreferences});
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), sync_service(),
                                         IOSPromoType::kPayment));
}

// Tests that ShouldShowIOSDesktopPromo returns false when sync service is null.
TEST_F(IOSPromoOnDesktopTest, PromoSyncPrefsSyncServiceNull) {
  EXPECT_FALSE(
      ShouldShowIOSDesktopPromo(profile(), nullptr, IOSPromoType::kPayment));
}

// Tests that ShouldShowIOSDesktopNtpPromo returns true when no promo has yet
// been shown.
TEST_F(IOSPromoOnDesktopTest, ShouldShowIOSDesktopNtpPromo) {
  EXPECT_TRUE(ShouldShowIOSDesktopNtpPromo(profile(), sync_service()));
}

// Tests that ShouldShowIOSDesktopNtpPromo returns false when the promotions are
// disabled.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopNtpPromoFalsePromotionsDisabled) {
  scoped_testing_local_state_.Get()->SetBoolean(prefs::kPromotionsEnabled,
                                                false);
  EXPECT_FALSE(ShouldShowIOSDesktopNtpPromo(profile(), sync_service()));
}

// Tests that ShouldShowIOSDesktopNtpPromo returns false when sync service is
// null.
TEST_F(IOSPromoOnDesktopTest, ShouldShowIOSDesktopNtpPromoSyncServiceNull) {
  EXPECT_FALSE(ShouldShowIOSDesktopNtpPromo(profile(), nullptr));
}

// Tests that ShouldShowIOSDesktopNtpPromo returns false when the user has
// already seen 10 promos.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopNtpPromoTestFalseTooManyImpressions) {
  base::Value::List timestamps;
  for (int i = 0; i < 10; i++) {
    timestamps.Append(base::TimeToValue(base::Time::Now() - base::Hours(1) +
                                        base::Seconds(i)));
  }
  prefs()->SetList(promos_prefs::kDesktopToiOSNtpPromoAppearanceTimestamps,
                   std::move(timestamps));
  EXPECT_FALSE(ShouldShowIOSDesktopNtpPromo(profile(), sync_service()));
}

// Tests that ShouldShowIOSDesktopNtpPromo returns false when the user has
// dismissed the promo.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopNtpPromoTestFalseUserDismissed) {
  prefs()->SetBoolean(promos_prefs::kDesktopToiOSNtpPromoDismissed, true);
  EXPECT_FALSE(ShouldShowIOSDesktopNtpPromo(profile(), sync_service()));
}

// Tests that ShouldShowIOSDesktopNtpPromo returns false when the another promo
// type has a too recent last impression.
TEST_F(
    IOSPromoOnDesktopTest,
    ShouldShowIOSDesktopNtpPromoTestFalseLastImpressionTooRecentForOtherPromo) {
  prefs()->SetTime(
      promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp,
      base::Time::Now());
  EXPECT_FALSE(ShouldShowIOSDesktopNtpPromo(profile(), sync_service()));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has already
// seen 3 promos for the given password promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopNtpPromoTestFalseTooManyImpressionsForOtherPromos) {
  prefs()->SetInteger(
      promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter, 12);
  EXPECT_FALSE(ShouldShowIOSDesktopNtpPromo(profile(), sync_service()));
}

// Tests that IOSDesktopNtpPromoShown sets the correct prefs.
TEST_F(IOSPromoOnDesktopTest, IOSDesktopNtpPromoShownTest) {
  // First impression
  base::Time before = base::Time::Now();
  IOSDesktopNtpPromoShown(prefs());
  base::Time after = base::Time::Now();

  ASSERT_EQ(
      prefs()
          ->GetList(promos_prefs::kDesktopToiOSNtpPromoAppearanceTimestamps)
          .size(),
      1u);
  ASSERT_GE(
      base::ValueToTime(
          prefs()
              ->GetList(promos_prefs::kDesktopToiOSNtpPromoAppearanceTimestamps)
              .back()),
      before);
  ASSERT_LE(
      base::ValueToTime(
          prefs()
              ->GetList(promos_prefs::kDesktopToiOSNtpPromoAppearanceTimestamps)
              .back()),
      after);

  // Second impression
  before = base::Time::Now();
  IOSDesktopNtpPromoShown(prefs());
  after = base::Time::Now();

  ASSERT_EQ(
      prefs()
          ->GetList(promos_prefs::kDesktopToiOSNtpPromoAppearanceTimestamps)
          .size(),
      2u);
  ASSERT_GE(
      base::ValueToTime(
          prefs()
              ->GetList(promos_prefs::kDesktopToiOSNtpPromoAppearanceTimestamps)
              .back()),
      before);
  ASSERT_LE(
      base::ValueToTime(
          prefs()
              ->GetList(promos_prefs::kDesktopToiOSNtpPromoAppearanceTimestamps)
              .back()),
      after);
}

}  // namespace promos_utils

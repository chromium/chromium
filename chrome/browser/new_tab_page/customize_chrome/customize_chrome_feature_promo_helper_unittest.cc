// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/customize_chrome/customize_chrome_feature_promo_helper.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/ui_base_features.h"

class CustomizeChromeFeaturePromoHelperTest : public BrowserWithTestWindowTest {
 protected:
  CustomizeChromeFeaturePromoHelperTest() {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    iph_feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHDesktopCustomizeChromeFeature});

    AddTab(browser(), GURL("chrome://newtab"));
    tab_ = browser()->tab_strip_model()->GetActiveWebContents();

    helper_ = std::make_unique<CustomizeChromeFeaturePromoHelper>();

    mock_tracker_ =
        static_cast<testing::NiceMock<feature_engagement::test::MockTracker>*>(
            feature_engagement::TrackerFactory::GetForBrowserContext(
                tab_->GetBrowserContext()));
  }

  void SetChromeRefresh2023() {
    iph_feature_list_.Reset();
    iph_feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature,
         features::kChromeRefresh2023, features::kChromeWebuiRefresh2023});
  }

  CustomizeChromeFeaturePromoHelper* helper() { return helper_.get(); }

  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    auto test_window = std::make_unique<TestBrowserWindow>();

    // This test only supports one window.
    DCHECK(!mock_promo_controller_);

    mock_promo_controller_ = static_cast<
        testing::NiceMock<user_education::test::MockFeaturePromoController>*>(
        test_window->SetFeaturePromoController(
            std::make_unique<testing::NiceMock<
                user_education::test::MockFeaturePromoController>>()));
    return test_window;
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories = {
        {feature_engagement::TrackerFactory::GetInstance(),
         base::BindRepeating(
             CustomizeChromeFeaturePromoHelperTest::MakeTestTracker)}};
    return factories;
  }

  content::WebContents* tab() { return tab_; }
  user_education::test::MockFeaturePromoController* mock_promo_controller() {
    return mock_promo_controller_;
  }
  feature_engagement::test::MockTracker* mock_tracker() {
    return mock_tracker_;
  }

 private:
  feature_engagement::test::ScopedIphFeatureList iph_feature_list_;
  raw_ptr<content::WebContents, DanglingUntriaged> tab_;
  raw_ptr<testing::NiceMock<feature_engagement::test::MockTracker>,
          DanglingUntriaged>
      mock_tracker_;
  raw_ptr<testing::NiceMock<user_education::test::MockFeaturePromoController>,
          DanglingUntriaged>
      mock_promo_controller_ = nullptr;
  std::unique_ptr<CustomizeChromeFeaturePromoHelper> helper_;

  static std::unique_ptr<KeyedService> MakeTestTracker(
      content::BrowserContext* context) {
    auto tracker = std::make_unique<
        testing::NiceMock<feature_engagement::test::MockTracker>>();
    return tracker;
  }
};

TEST_F(CustomizeChromeFeaturePromoHelperTest,
       RecordCustomizeChromeFeatureUsage) {
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(feature_engagement::events::kCustomizeChromeOpened))
      .Times(1);
  helper()->RecordCustomizeChromeFeatureUsage(tab());
}

TEST_F(CustomizeChromeFeaturePromoHelperTest,
       MaybeShowCustomizeChromeFeaturePromoHelper) {
  EXPECT_CALL(*mock_promo_controller(),
              MaybeShowPromo(user_education::test::MatchFeaturePromoParams(
                  feature_engagement::kIPHDesktopCustomizeChromeFeature)))
      .Times(1)
      .WillOnce(testing::Return(user_education::FeaturePromoResult::Success()));
  helper()->SetDefaultSearchProviderIsGoogleForTesting(true);
  helper()->MaybeShowCustomizeChromeFeaturePromo(tab());
}

TEST_F(CustomizeChromeFeaturePromoHelperTest,
       MaybeShowCustomizeChromeFeaturePromoHelperNonGoogle) {
  EXPECT_CALL(*mock_promo_controller(), MaybeShowPromo(testing::_)).Times(0);
  helper()->SetDefaultSearchProviderIsGoogleForTesting(false);
  helper()->MaybeShowCustomizeChromeFeaturePromo(tab());
}

TEST_F(CustomizeChromeFeaturePromoHelperTest,
       CloseCustomizeChromeFeaturePromoHelper) {
  EXPECT_CALL(
      *mock_promo_controller(),
      EndPromo(
          testing::Ref(feature_engagement::kIPHDesktopCustomizeChromeFeature),
          testing::_))
      .Times(1)
      .WillOnce(testing::Return(user_education::FeaturePromoResult::Success()));
  helper()->CloseCustomizeChromeFeaturePromo(tab());
}

TEST_F(CustomizeChromeFeaturePromoHelperTest,
       MaybeShowCustomizeChromeRefreshFeaturePromoHelper) {
  SetChromeRefresh2023();
  EXPECT_CALL(
      *mock_promo_controller(),
      MaybeShowPromo(user_education::test::MatchFeaturePromoParams(
          feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature)))
      .Times(1)
      .WillOnce(testing::Return(user_education::FeaturePromoResult::Success()));
  helper()->SetDefaultSearchProviderIsGoogleForTesting(true);
  helper()->MaybeShowCustomizeChromeFeaturePromo(tab());
}

TEST_F(CustomizeChromeFeaturePromoHelperTest,
       MaybeShowCustomizeChromeRefreshFeaturePromoHelperNonGoogle) {
  SetChromeRefresh2023();
  EXPECT_CALL(*mock_promo_controller(), MaybeShowPromo(testing::_)).Times(0);
  helper()->SetDefaultSearchProviderIsGoogleForTesting(false);
  helper()->MaybeShowCustomizeChromeFeaturePromo(tab());
}

TEST_F(CustomizeChromeFeaturePromoHelperTest,
       CloseCustomizeChromeRefreshFeaturePromoHelper) {
  SetChromeRefresh2023();
  EXPECT_CALL(
      *mock_promo_controller(),
      EndPromo(
          testing::Ref(
              feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature),
          testing::_))
      .Times(1)
      .WillOnce(testing::Return(user_education::FeaturePromoResult::Success()));
  helper()->CloseCustomizeChromeFeaturePromo(tab());
}

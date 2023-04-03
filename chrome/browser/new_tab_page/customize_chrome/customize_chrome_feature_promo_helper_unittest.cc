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
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class CustomizeChromeFeaturePromoHelperTest : public BrowserWithTestWindowTest {
 protected:
  CustomizeChromeFeaturePromoHelperTest() {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    iph_feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHDesktopCustomizeChromeFeature});

    AddTab(browser(), GURL("chrome://newtab"));
    tab_ = browser()->tab_strip_model()->GetActiveWebContents();

    mock_tracker_ =
        static_cast<testing::NiceMock<feature_engagement::test::MockTracker>*>(
            feature_engagement::TrackerFactory::GetForBrowserContext(
                tab_->GetBrowserContext()));
  }

  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    auto test_window = std::make_unique<TestBrowserWindow>();

    // This test only supports one window.
    DCHECK(!mock_promo_controller_);

    mock_promo_controller_ =
        static_cast<user_education::test::MockFeaturePromoController*>(
            test_window->SetFeaturePromoController(
                std::make_unique<
                    user_education::test::MockFeaturePromoController>()));
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
  raw_ptr<content::WebContents> tab_;
  raw_ptr<testing::NiceMock<feature_engagement::test::MockTracker>>
      mock_tracker_;
  raw_ptr<user_education::test::MockFeaturePromoController>
      mock_promo_controller_ = nullptr;

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
  CustomizeChromeFeaturePromoHelper customize_chrome_feature_promo_helper;
  customize_chrome_feature_promo_helper.RecordCustomizeChromeFeatureUsage(
      tab());
}

TEST_F(CustomizeChromeFeaturePromoHelperTest,
       MaybeShowCustomizeChromeFeaturePromoHelper) {
  EXPECT_CALL(
      *mock_promo_controller(),
      MaybeShowPromo(
          testing::Ref(feature_engagement::kIPHDesktopCustomizeChromeFeature),
          testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  CustomizeChromeFeaturePromoHelper customize_chrome_feature_promo_helper;
  customize_chrome_feature_promo_helper.MaybeShowCustomizeChromeFeaturePromo(
      tab());
}

TEST_F(CustomizeChromeFeaturePromoHelperTest,
       CloseCustomizeChromeFeaturePromoHelper) {
  EXPECT_CALL(*mock_promo_controller(),
              EndPromo(testing::Ref(
                  feature_engagement::kIPHDesktopCustomizeChromeFeature)))
      .Times(1)
      .WillOnce(testing::Return(true));
  CustomizeChromeFeaturePromoHelper customize_chrome_feature_promo_helper;
  customize_chrome_feature_promo_helper.CloseCustomizeChromeFeaturePromo(tab());
}

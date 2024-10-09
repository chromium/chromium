// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"

#include "build/build_config.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class NewTabPageFeaturePromoHelperTest : public BrowserWithTestWindowTest {
 protected:
  NewTabPageFeaturePromoHelperTest() {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    iph_feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHDesktopCustomizeChromeFeature});

    AddTab(browser(), GURL("chrome://newtab"));
    tab_ = browser()->tab_strip_model()->GetActiveWebContents();

    helper_ = std::make_unique<NewTabPageFeaturePromoHelper>();

    MaybeRegisterChromeFeaturePromos(
        UserEducationServiceFactory::GetForBrowserContext(
            tab_->GetBrowserContext())
            ->feature_promo_registry());
  }

  NewTabPageFeaturePromoHelper* helper() { return helper_.get(); }

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

  content::WebContents* tab() { return tab_; }
  user_education::test::MockFeaturePromoController* mock_promo_controller() {
    return mock_promo_controller_;
  }
 private:
  feature_engagement::test::ScopedIphFeatureList iph_feature_list_;
  raw_ptr<content::WebContents, DanglingUntriaged> tab_;
  raw_ptr<testing::NiceMock<user_education::test::MockFeaturePromoController>,
          DanglingUntriaged>
      mock_promo_controller_ = nullptr;
  std::unique_ptr<NewTabPageFeaturePromoHelper> helper_;
};

// In CFT mode, there are often no User Ed controllers.
#if !BUILDFLAG(CHROME_FOR_TESTING)
TEST_F(NewTabPageFeaturePromoHelperTest, RecordFeatureUsage_CustomizeChrome) {
  // By default let through all calls to endpromo.
  EXPECT_CALL(*mock_promo_controller(), EndPromo(testing::_, testing::_))
      .WillRepeatedly(
          testing::Return(user_education::FeaturePromoResult::Success()));
  // Check for this call specifically.
  EXPECT_CALL(
      *mock_promo_controller(),
      EndPromo(
          testing::Ref(feature_engagement::kIPHDesktopCustomizeChromeFeature),
          testing::_))
      .Times(1)
      .WillOnce(testing::Return(user_education::FeaturePromoResult::Success()));
  helper()->RecordPromoFeatureUsageAndClosePromo(
      feature_engagement::kIPHDesktopCustomizeChromeFeature, tab());
}
#endif  // !BUILDFLAG(CHROME_FOR_TESTING)

TEST_F(NewTabPageFeaturePromoHelperTest,
       MaybeShowFeaturePromo_CustomizeChrome) {
  EXPECT_CALL(*mock_promo_controller(),
              MaybeShowPromo(user_education::test::MatchFeaturePromoParams(
                  feature_engagement::kIPHDesktopCustomizeChromeFeature)))
      .Times(1);
  helper()->SetDefaultSearchProviderIsGoogleForTesting(true);
  helper()->MaybeShowFeaturePromo(
      feature_engagement::kIPHDesktopCustomizeChromeFeature, tab());
}

TEST_F(NewTabPageFeaturePromoHelperTest,
       MaybeShowFeaturePromo_NonGoogle_CustomizeChrome) {
  EXPECT_CALL(*mock_promo_controller(), MaybeShowPromo(testing::_)).Times(0);
  helper()->SetDefaultSearchProviderIsGoogleForTesting(false);
  helper()->MaybeShowFeaturePromo(
      feature_engagement::kIPHDesktopCustomizeChromeFeature, tab());
}

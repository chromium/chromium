// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

class NewTabPageFeaturePromoHelperTest : public BrowserWithTestWindowTest {
 protected:
  NewTabPageFeaturePromoHelperTest() = default;

  void SetUp() override {
    // Override the factory before the browser window is created.
    user_ed_override_ =
        BrowserWindowFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting(
                base::BindRepeating([](BrowserWindowInterface& window) {
                  return std::make_unique<MockBrowserUserEducationInterface>(
                      &window);
                }));

    BrowserWithTestWindowTest::SetUp();

    iph_feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHDesktopCustomizeChromeExperimentFeature});

    AddTab(browser(), GURL("chrome://newtab"));
    tab_ = browser()->tab_strip_model()->GetActiveWebContents();

    helper_ = std::make_unique<NewTabPageFeaturePromoHelper>();

    MaybeRegisterChromeFeaturePromos(
        UserEducationServiceFactory::GetForBrowserContext(
            tab_->GetBrowserContext())
            ->feature_promo_registry());
  }

  MockBrowserUserEducationInterface* user_education() {
    return static_cast<MockBrowserUserEducationInterface*>(
        BrowserUserEducationInterface::From(browser()));
  }

  NewTabPageFeaturePromoHelper* helper() { return helper_.get(); }

  content::WebContents* tab() { return tab_; }

 private:
  feature_engagement::test::ScopedIphFeatureList iph_feature_list_;
  raw_ptr<content::WebContents, DanglingUntriaged> tab_;
  std::unique_ptr<NewTabPageFeaturePromoHelper> helper_;
  ui::UserDataFactory::ScopedOverride user_ed_override_;
};

// In CFT mode, there are often no User Ed controllers.
#if !BUILDFLAG(CHROME_FOR_TESTING)
TEST_F(NewTabPageFeaturePromoHelperTest, RecordFeatureUsage_CustomizeChrome) {
  // By default let through all calls to endpromo.
  EXPECT_CALL(*user_education(),
              NotifyFeaturePromoFeatureUsed(testing::_, testing::_))
      .Times(testing::AnyNumber());
  // Check for this call specifically.
  EXPECT_CALL(
      *user_education(),
      NotifyFeaturePromoFeatureUsed(
          testing::Ref(
              feature_engagement::kIPHDesktopCustomizeChromeExperimentFeature),
          testing::_))
      .Times(1);
  helper()->RecordPromoFeatureUsageAndClosePromo(
      feature_engagement::kIPHDesktopCustomizeChromeExperimentFeature, tab());
}
#endif  // !BUILDFLAG(CHROME_FOR_TESTING)

TEST_F(NewTabPageFeaturePromoHelperTest,
       MaybeShowFeaturePromo_CustomizeChrome) {
  EXPECT_CALL(
      *user_education(),
      MaybeShowFeaturePromo(user_education::test::MatchFeaturePromoParams(
          feature_engagement::kIPHDesktopCustomizeChromeExperimentFeature)))
      .Times(1);
  helper()->SetDefaultSearchProviderIsGoogleForTesting(true);
  helper()->MaybeShowFeaturePromo(
      feature_engagement::kIPHDesktopCustomizeChromeExperimentFeature, tab());
}

TEST_F(NewTabPageFeaturePromoHelperTest,
       MaybeShowFeaturePromo_NonGoogle_CustomizeChrome) {
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo).Times(0);
  helper()->SetDefaultSearchProviderIsGoogleForTesting(false);
  helper()->MaybeShowFeaturePromo(
      feature_engagement::kIPHDesktopCustomizeChromeExperimentFeature, tab());
}

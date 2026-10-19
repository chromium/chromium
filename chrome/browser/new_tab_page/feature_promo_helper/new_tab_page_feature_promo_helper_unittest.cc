// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/side_panel/mock_side_panel_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/actions/actions.h"
#include "ui/base/mojom/themes.mojom.h"

class NewTabPageFeaturePromoHelperCustomizeChromeAutoOpenUnitTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    webui::SetBrowserWindowInterface(web_contents(),
                                     &mock_browser_window_interface_);
    actions::ActionManager::GetForTesting().AddAction(
        actions::ActionItem::Builder()
            .SetActionId(kActionSidePanelShowCustomizeChrome)
            .CopyAddressTo(&show_customize_chrome_action_)
            .Build());
  }

  void TearDown() override {
    show_customize_chrome_action_ = nullptr;
    actions::ActionManager::ResetForTesting();
    webui::SetBrowserWindowInterface(web_contents(), nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ntp_features::kNtpCustomizeChromeAutoOpen};
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  testing::NiceMock<MockSidePanelUI> mock_side_panel_ui_{
      mock_browser_window_interface_.GetUnownedUserDataHost()};

 protected:
  testing::NiceMock<MockBrowserUserEducationInterface>
      mock_user_education_interface_{&mock_browser_window_interface_};
  raw_ptr<actions::ActionItem> show_customize_chrome_action_ = nullptr;
  base::HistogramTester histogram_tester_;
  NewTabPageFeaturePromoHelper helper_;
};

TEST_F(NewTabPageFeaturePromoHelperCustomizeChromeAutoOpenUnitTest,
       DontOpenPanelWhenUserCustomizedChromeAlready) {
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile());
  theme_service->SetUserColorAndBrowserColorVariant(
      SkColorSetRGB(0x00, 0x00, 0x00),
      ui::mojom::BrowserColorVariant::kVibrant);

  EXPECT_CALL(mock_user_education_interface_, MaybeShowFeaturePromo).Times(0);

  helper_.MaybeTriggerAutomaticCustomizeChromePromo(web_contents());

  EXPECT_EQ(show_customize_chrome_action_->GetInvokeCount(), 0);
  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kChromeCustomizedAlready, 1);
}

TEST_F(NewTabPageFeaturePromoHelperCustomizeChromeAutoOpenUnitTest,
       DontOpenPanelWhenCustomizeButtonWasClickedBefore) {
  profile()->GetPrefs()->SetInteger(prefs::kNtpCustomizeChromeButtonOpenCount,
                                    1);

  EXPECT_CALL(mock_user_education_interface_, MaybeShowFeaturePromo).Times(0);

  helper_.MaybeTriggerAutomaticCustomizeChromePromo(web_contents());

  EXPECT_EQ(show_customize_chrome_action_->GetInvokeCount(), 0);
  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kCustomizeChromeOpenedByUser, 1);
}

TEST_F(NewTabPageFeaturePromoHelperCustomizeChromeAutoOpenUnitTest,
       DontOpenPanelWhenReachedTotalMaxCountAlready) {
  profile()->GetPrefs()->SetInteger(
      prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount, 5);

  EXPECT_CALL(mock_user_education_interface_, MaybeShowFeaturePromo).Times(0);

  helper_.MaybeTriggerAutomaticCustomizeChromePromo(web_contents());

  EXPECT_EQ(show_customize_chrome_action_->GetInvokeCount(), 0);
  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kReachedTotalMaxCountAlready, 1);
}

TEST_F(NewTabPageFeaturePromoHelperCustomizeChromeAutoOpenUnitTest,
       DontOpenPanelWhenPromoAutoopenedInTheSameSession) {
  profile()->GetPrefs()->SetBoolean(prefs::kNtpCustomizeChromeIPHAutoOpened,
                                    true);

  EXPECT_CALL(
      mock_user_education_interface_,
      MaybeShowFeaturePromo(user_education::test::MatchFeaturePromoParams(
          feature_engagement::kIPHDesktopCustomizeChromeAutoOpenFeature)))
      .Times(1);

  helper_.MaybeTriggerAutomaticCustomizeChromePromo(web_contents());
  EXPECT_EQ(show_customize_chrome_action_->GetInvokeCount(), 1);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kCanShowPromo, 1);

  helper_.MaybeTriggerAutomaticCustomizeChromePromo(web_contents());
  EXPECT_EQ(show_customize_chrome_action_->GetInvokeCount(), 1);
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kReachedSessionMaxCountAlready, 1);
}

TEST_F(NewTabPageFeaturePromoHelperCustomizeChromeAutoOpenUnitTest,
       ShouldShowSidePanelForTheFirstTimeOnlyWhenIPHIsShown) {
  EXPECT_CALL(
      mock_user_education_interface_,
      MaybeShowFeaturePromo(user_education::test::MatchFeaturePromoParams(
          feature_engagement::kIPHDesktopCustomizeChromeAutoOpenFeature)))
      .WillOnce([](user_education::FeaturePromoParams params) {
        std::move(params.show_promo_result_callback)
            .Run(user_education::FeaturePromoResult::kBlockedByPromo);
        return false;
      })
      .WillOnce([](user_education::FeaturePromoParams params) {
        std::move(params.show_promo_result_callback)
            .Run(user_education::FeaturePromoResult::Success());
        return true;
      });

  helper_.MaybeTriggerAutomaticCustomizeChromePromo(web_contents());
  EXPECT_EQ(show_customize_chrome_action_->GetInvokeCount(), 0);
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kNtpCustomizeChromeIPHAutoOpened));
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount),
            0);

  helper_.MaybeTriggerAutomaticCustomizeChromePromo(web_contents());
  EXPECT_EQ(show_customize_chrome_action_->GetInvokeCount(), 1);
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kNtpCustomizeChromeIPHAutoOpened));
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount),
            1);
}

TEST_F(NewTabPageFeaturePromoHelperCustomizeChromeAutoOpenUnitTest,
       ShouldShowSidePanelForTheSecondTimeIndependentlyOfIPH) {
  profile()->GetPrefs()->SetBoolean(prefs::kNtpCustomizeChromeIPHAutoOpened,
                                    true);
  profile()->GetPrefs()->SetInteger(
      prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount, 1);

  EXPECT_CALL(
      mock_user_education_interface_,
      MaybeShowFeaturePromo(user_education::test::MatchFeaturePromoParams(
          feature_engagement::kIPHDesktopCustomizeChromeAutoOpenFeature)))
      .WillOnce(testing::Return(false));

  helper_.MaybeTriggerAutomaticCustomizeChromePromo(web_contents());

  EXPECT_EQ(show_customize_chrome_action_->GetInvokeCount(), 1);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount),
            2);
  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.CustomizeChromePromoEligibility",
      NTPCustomizeChromePromoEligibility::kCanShowPromo, 1);
}

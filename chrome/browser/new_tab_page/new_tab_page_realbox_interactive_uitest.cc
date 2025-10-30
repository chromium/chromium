// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/aim_entrypoint_fieldtrial.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/mock_os_settings_provider.h"

// For local pixel test debugging, use the following flag combinations.
//
// 1. To run the pixel test:
//    --gtest_filter=*<TEST_NAME>* \
//    --browser-ui-tests-verify-pixels \
//    --enable-pixel-output-in-tests
//
// 2. To see the UI as it is being screenshot:
//    Replace `--enable-pixel-output-in-tests` with
//    `--test-launcher-interactive`. This will freeze the test just after the
//    screenshot is taken. Dismiss the UI to continue the test.
//
// 3. To save the screenshot to a local directory:
//    Add the following flags to the basic command:
//    --bypass-skia-gold-functionality  \
//    --skia-gold-local-png-write-directory=<TEMP_DIRECTORY>

namespace {
using ntp_realbox::RealboxLayoutMode;
using ::testing::ValuesIn;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpElementId);

// Contains variables on which these tests may be parameterized. This approach
// makes it easy to build sets of relevant tests, vs. the brute-force
// testing::Combine() approach.
struct NtpRealboxUiTestParams {
  RealboxLayoutMode layout_mode = RealboxLayoutMode::kCompact;
  bool compose_button_enabled = true;
  ui::NativeTheme::PreferredColorScheme color_scheme =
      ui::NativeTheme::PreferredColorScheme::kLight;
  bool rtl = false;

  std::string ToString() const {
    std::ostringstream oss;
    oss << RealboxLayoutModeToString(layout_mode);
    if (!compose_button_enabled) {
      oss << "_compose_disabled";
    }
    if (color_scheme == ui::NativeTheme::PreferredColorScheme::kDark) {
      oss << "_dark";
    }
    if (rtl) {
      oss << "_rtl";
    }
    return oss.str();
  }
};

std::unique_ptr<KeyedService> BuildMockAimServiceEligibilityServiceInstance(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<testing::NiceMock<MockAimEligibilityService>>
      mock_aim_eligibility_service =
          std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
              CHECK_DEREF(profile->GetPrefs()),
              /*template_url_service=*/nullptr,
              /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);

  ON_CALL(*mock_aim_eligibility_service, IsAimEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_eligibility_service, IsAimLocallyEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_eligibility_service, IsServerEligibilityEnabled())
      .WillByDefault(testing::Return(true));

  return std::move(mock_aim_eligibility_service);
}

}  // namespace

class NtpRealboxUiTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest>,
      public testing::WithParamInterface<NtpRealboxUiTestParams> {
 public:
  NtpRealboxUiTest() = default;
  ~NtpRealboxUiTest() override = default;

  void SetUp() override {
    // Enable Realbox Next and specify layout mode.
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    base::FieldTrialParams realbox_params;
    realbox_params[ntp_realbox::kRealboxLayoutMode.name] = [=]() {
      switch (GetParam().layout_mode) {
        case RealboxLayoutMode::kTallBottomContext:
          return ntp_realbox::kRealboxLayoutModeTallBottomContext;
        case RealboxLayoutMode::kTallTopContext:
          return ntp_realbox::kRealboxLayoutModeTallTopContext;
        case RealboxLayoutMode::kCompact:
          return ntp_realbox::kRealboxLayoutModeCompact;
      }
    }();
    enabled_features.emplace_back(ntp_realbox::kNtpRealboxNext, realbox_params);

    // Disable NTP features that load asynchronously to prevent page shifts.
    // TODO(crbug.com/452928336): Wait for a signal that the NTP's layout is
    // complete instead.
    std::vector<base::test::FeatureRef> disabled_features = {
        user_education::features::kEnableNtpBrowserPromos,
        ntp_features::kNtpFooter,
        ntp_features::kNtpMiddleSlotPromo,
        ntp_features::kNtpLogo,
        ntp_features::kNtpOneGoogleBar,
        ntp_features::kNtpShortcuts};

    // Conditionally enable or disable Compose Entrypoint features.
    if (GetParam().compose_button_enabled) {
      enabled_features.push_back(
          {ntp_composebox::kNtpSearchboxComposeEntrypoint, {}});
      enabled_features.push_back({ntp_composebox::kNtpComposebox, {}});
    } else {
      disabled_features.push_back(ntp_composebox::kNtpComposebox);
      disabled_features.push_back(
          ntp_composebox::kNtpSearchboxComposeEntrypoint);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);

    WebUiInteractiveTestMixin::SetUp();
  }

  void SetUpOnMainThread() override {
    WebUiInteractiveTestMixin::SetUpOnMainThread();
    // Sanity check that the NtpRealboxUiTestParams setup actually took; if it
    // didn't, then we can't accurately perform the test.
    ASSERT_EQ(RealboxLayoutModeToString(ntp_realbox::kRealboxLayoutMode.Get()),
              RealboxLayoutModeToString(GetParam().layout_mode));
    ASSERT_EQ(ntp_composebox::IsNtpSearchboxComposeEntrypointEnabled(
                  browser()->profile()),
              GetParam().compose_button_enabled);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InteractiveBrowserTest::SetUpBrowserContextKeyedServices(context);
    if (GetParam().compose_button_enabled) {
      AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
          context,
          base::BindOnce(BuildMockAimServiceEligibilityServiceInstance));
    }
  }

  // TODO(crbug.com/452000330): Pull this out into the common Web UI interaction
  // utility helper.
  auto WaitForAndScrollToElement(
      const ui::ElementIdentifier& ntp_id,
      const WebContentsInteractionTestUtil::DeepQuery& query) {
    auto steps = Steps(WaitForElementToRender(ntp_id, query),
                       ScrollIntoView(ntp_id, query));
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

 protected:
  ui::MockOsSettingsProvider& os_settings_provider() {
    return os_settings_provider_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ui::MockOsSettingsProvider os_settings_provider_;
};

// Screenshot the realbox UI across the available presentation styles, along
// with other variables that warrant pixel-style validation.
INSTANTIATE_TEST_SUITE_P(
    ,
    NtpRealboxUiTest,
    ValuesIn(std::vector<NtpRealboxUiTestParams>{
        // Compact, compose disabled, light mode, LTR
        {
            .layout_mode = RealboxLayoutMode::kCompact,
            .compose_button_enabled = false,
        },
        // Compact, compose enabled, light mode, LTR
        {
            .layout_mode = RealboxLayoutMode::kCompact,
        },
        // Compact, compose enabled, dark mode, RTL
        {
            .layout_mode = RealboxLayoutMode::kCompact,
            .color_scheme = ui::NativeTheme::PreferredColorScheme::kDark,
            .rtl = true,
        },
        // Tall bottom, compose enabled, light mode, LTR
        {
            .layout_mode = RealboxLayoutMode::kTallBottomContext,
        },
        // Tall bottom, compose enabled, dark mode, RTL
        {
            .layout_mode = RealboxLayoutMode::kTallBottomContext,
            .color_scheme = ui::NativeTheme::PreferredColorScheme::kDark,
            .rtl = true,
        },
        // Tall top, compose enabled, light mode, LTR
        {
            .layout_mode = RealboxLayoutMode::kTallTopContext,
        },
        // Tall top, compose enabled, dark mode, RTL
        {
            .layout_mode = RealboxLayoutMode::kTallTopContext,
            .color_scheme = ui::NativeTheme::PreferredColorScheme::kDark,
            .rtl = true,
        },
    }),
    [](const testing::TestParamInfo<NtpRealboxUiTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(NtpRealboxUiTest, Screenshots) {
  // Force a consistent window size to exercise realbox layout within New Tab
  // Page bounds.
  auto screen_size = gfx::Size(1000, 1200);
  BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetSize(
      screen_size);

  os_settings_provider().SetPreferredColorScheme(GetParam().color_scheme);

  if (GetParam().rtl) {
    base::i18n::SetRTLForTesting(true);
  }

  // Disable compose button animation to prevent screenshot variations.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kNtpComposeButtonShownCountPrefName,
      ntp_composebox::FeatureConfig::Get()
          .config.entry_point()
          .num_page_load_animations());

  const DeepQuery kContent = {"ntp-app", "#content"};
  const DeepQuery kRealbox = {"ntp-app", "cr-searchbox", "#inputWrapper"};
  const DeepQuery kContextMenuEntrypoint = {
      "ntp-app", "cr-searchbox", "contextual-entrypoint-and-carousel"};

  RunTestSequence(
      // 1. Open 1P new tab page.
      AddInstrumentedTab(kNtpElementId, GURL(chrome::kChromeUINewTabURL)),
      // 2. If compose button is enabled, wait for it to render. Otherwise, wait
      // on the realbox to render.
      If([&]() { return GetParam().compose_button_enabled; },
         Then(WaitForAndScrollToElement(kNtpElementId, kContextMenuEntrypoint)),
         Else(WaitForAndScrollToElement(kNtpElementId, kRealbox))),
      // 3. Skip rest of the test on platforms where screenshots aren't
      // captured.
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshots not captured on this platform."),
      // 4. Screenshot the content.
      // TODO(crbug.com/452928336): Wait for a signal that the NTP's layout
      // is complete and take a screenshot of the searchbox's container instead.
      ScreenshotWebUi(kNtpElementId, kContent,
                      /*screenshot_name=*/std::string(),
                      /*baseline_cl=*/"7085674"));
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/check_deref.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/native_theme/mock_os_settings_provider.h"

// To debug locally, you can run the test via:
// `out/Default/interactive_ui_tests
// --gtest_filter="*<TEST_NAME>*" --test-launcher-interactive`. The
// `--test-launcher-interactive` flag will pause the test at the very end, after
// the screenshot would've been taken, allowing you to inspect the UI and debug.
//
// To generate an actual screenshot locally, you can run the test with
// `out/Default/interactive_ui_tests
// --gtest_filter="*<TEST_NAME>*" --browser-ui-tests-verify-pixels
// --enable-pixel-output-in-tests --test-launcher-retry-limit=0
// --ui-test-action-timeout=100000
// --skia-gold-local-png-write-directory="/tmp/pixel_test_output"
// --bypass-skia-gold-functionality`. The PNG of the screenshot will be saved to
// the `/tmp/pixel_test_output` directory.

namespace {
using ntp_realbox::RealboxLayoutMode;
using ::testing::ValuesIn;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kGooglePageId);

static constexpr std::string_view kModelFastLabel = "Fast";
static constexpr std::string_view kModelAutoLabel = "Auto";
static constexpr std::string_view kModelProLabel = "Pro";
static constexpr std::string_view kHintText = "Ask anything";
static constexpr std::string_view kInputTypeAddImage = "Add image";
static constexpr std::string_view kInputTypeAddFile = "Add file";
static constexpr std::string_view kToolCreateImages = "Create images";
static constexpr std::string_view kToolCanvas = "Canvas";

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
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service =
      std::make_unique<MockAimEligibilityService>(
          CHECK_DEREF(profile->GetPrefs()), /*template_url_service=*/nullptr,
          /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr,
          AimEligibilityService::Configuration{});

  auto* rule_set = mock_aim_eligibility_service->config().mutable_rule_set();
  for (const auto input_type : {omnibox::InputType::INPUT_TYPE_LENS_IMAGE,
                                omnibox::InputType::INPUT_TYPE_LENS_FILE}) {
    rule_set->add_allowed_input_types(input_type);
  }
  for (const auto tool : {omnibox::ToolMode::TOOL_MODE_CANVAS,
                          omnibox::ToolMode::TOOL_MODE_IMAGE_GEN}) {
    rule_set->add_allowed_tools(tool);
  }
  for (const auto model : {omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR,
                           omnibox::ModelMode::MODEL_MODE_GEMINI_PRO}) {
    rule_set->add_allowed_models(model);
  }

  auto* regular_config =
      mock_aim_eligibility_service->config().add_model_configs();
  regular_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  regular_config->set_menu_label(std::string(kModelFastLabel));

  auto* pro_config = mock_aim_eligibility_service->config().add_model_configs();
  pro_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  pro_config->set_menu_label(std::string(kModelProLabel));
  mock_aim_eligibility_service->config().set_hint_text(std::string(kHintText));

  ON_CALL(*mock_aim_eligibility_service, IsAimEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_eligibility_service, IsAimLocallyEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_eligibility_service, IsServerEligibilityEnabled())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_eligibility_service, IsCanvasEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_eligibility_service, IsDeepSearchEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_eligibility_service, IsCreateImagesEligible())
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
    enabled_features.emplace_back(omnibox::kAimEnabled,
                                  base::FieldTrialParams());
    enabled_features.emplace_back(omnibox::kAimServerEligibilityEnabled,
                                  base::FieldTrialParams());
    enabled_features.emplace_back(omnibox::kAimUsePecApi,
                                  base::FieldTrialParams());

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
      enabled_features.push_back({ntp_composebox::kNtpComposebox, {}});
    } else {
      disabled_features.push_back(ntp_composebox::kNtpComposebox);
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
// TODO(crbug.com/454668186): Test fails on Windows builders for Compact and
// Compact_dark_rtl
#if !BUILDFLAG(IS_WIN)
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
#endif
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

using NtpRealboxNextUiTest = NtpRealboxUiTest;

INSTANTIATE_TEST_SUITE_P(
    ,
    NtpRealboxNextUiTest,
    ValuesIn(std::vector<NtpRealboxUiTestParams>{
        {
            .layout_mode = RealboxLayoutMode::kCompact,
        },
    }),
    [](const testing::TestParamInfo<NtpRealboxUiTestParams>& info) {
      return info.param.ToString();
    });

// TODO(crbug.com/454761015): Re-enable after fixing.
IN_PROC_BROWSER_TEST_P(NtpRealboxUiTest, DISABLED_Screenshots) {
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

  const DeepQuery kSearchboxContainer = {"ntp-app", "#content"};
  const DeepQuery kRealbox = {"ntp-app", "cr-searchbox", "#inputWrapper"};
  const DeepQuery kContextMenuEntrypoint = {
      "ntp-app", "cr-searchbox", "#context"};

  RunTestSequence(
      // 1. Open 1P new tab page.
      AddInstrumentedTab(kNtpElementId, GURL(chrome::kChromeUINewTabURL)),
      // 2. If compose button is enabled, wait for it to render. Otherwise, wait
      // on the realbox to render.
      If([&]() { return GetParam().compose_button_enabled; },
         Then(WaitForAndScrollToElement(kNtpElementId, kContextMenuEntrypoint)),
         Else(WaitForAndScrollToElement(kNtpElementId, kRealbox))),
      // 3. Screenshot the content.
      // TODO(crbug.com/452928336): Wait for a signal that the NTP's layout is
      // complete and take a screenshot of the searchbox's container instead.
      Steps(
          SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                                  "Screenshots not captured on this platform."),
          ScreenshotWebUi(kNtpElementId, kSearchboxContainer,
                          /*screenshot_name=*/std::string(),
                          /*baseline_cl=*/"7055903")));
}

IN_PROC_BROWSER_TEST_P(NtpRealboxNextUiTest, AimButtonOpensComposebox) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kComposeboxDialogOpenEvent);

  const DeepQuery kRealbox = {"ntp-app", "cr-searchbox", "#inputWrapper"};
  const DeepQuery kComposeButton = {"ntp-app", "cr-searchbox",
                                    "#composeButton"};
  const DeepQuery kComposeboxDialog = {"ntp-app", "#composeboxDialog"};

  WebContentsInteractionTestUtil::StateChange composebox_dialog_open;
  composebox_dialog_open.event = kComposeboxDialogOpenEvent;
  composebox_dialog_open.where = kComposeboxDialog;
  composebox_dialog_open.test_function =
      "(el) => el && el.hasAttribute('open')";

  RunTestSequence(
      // 1. Open a site.
      AddInstrumentedTab(kGooglePageId, GURL("https://www.google.com")),
      // 2. Load NTP.
      AddInstrumentedTab(kNtpElementId, GURL(chrome::kChromeUINewTabURL)),
      // 3. Assert NTP has loaded by waiting for the realbox and compose button
      // to render.
      WaitForElementToRender(kNtpElementId, kRealbox),
      WaitForElementToRender(kNtpElementId, kComposeButton),
      // 4. Click on the compose button.
      ClickElement(kNtpElementId, kComposeButton),
      // 5. Observe/assert that the contextual dialog is open.
      WaitForStateChange(kNtpElementId, composebox_dialog_open));
}

IN_PROC_BROWSER_TEST_P(NtpRealboxNextUiTest,
                       ContextualEntrypointMenuHasOptions) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kContextMenuOpenEvent);

  const DeepQuery kRealbox = {"ntp-app", "cr-searchbox", "#inputWrapper"};
  const DeepQuery kContextualEntrypoint = {"ntp-app",
                                           "cr-searchbox",
                                           "#context",
                                           "#entrypointButton",
                                           "#entrypoint"};
  const DeepQuery kContextMenuDialog = {"ntp-app",
                                        "cr-searchbox",
                                        "#context",
                                        "#menu",
                                        "#menu",
                                        "#dialog"};

  WebContentsInteractionTestUtil::StateChange context_menu_open;
  context_menu_open.event = kContextMenuOpenEvent;
  context_menu_open.where = kContextMenuDialog;
  context_menu_open.test_function = "(el) => el && el.open";

  const DeepQuery kImageUploadItem = {"ntp-app",
                                      "cr-searchbox",
                                      "#context",
                                      "#menu",
                                      "#imageUpload"};
  const DeepQuery kFileUploadItem = {"ntp-app",
                                     "cr-searchbox",
                                     "#context",
                                     "#menu",
                                     "#fileUpload"};
  const DeepQuery kCreateImagesItem = {"ntp-app",
                                       "cr-searchbox",
                                       "#context",
                                       "#menu",
                                       "button[data-mode='4']"};
  const DeepQuery kCanvasItem = {"ntp-app",
                                 "cr-searchbox",
                                 "#context",
                                 "#menu",
                                 "button[data-mode='2']"};
  const DeepQuery kFastModelItem = {"ntp-app",
                                    "cr-searchbox",
                                    "#context",
                                    "#menu",
                                    "button[data-model='1']"};
  const DeepQuery kProModelItem = {"ntp-app",
                                   "cr-searchbox",
                                   "#context",
                                   "#menu",
                                   "button[data-model='2']"};

  RunTestSequence(
      AddInstrumentedTab(kNtpElementId, GURL(chrome::kChromeUINewTabURL)),
      WaitForElementToRender(kNtpElementId, kRealbox),
      WaitForElementToRender(kNtpElementId, kContextualEntrypoint),
      ClickElement(kNtpElementId, kContextualEntrypoint),
      WaitForStateChange(kNtpElementId, context_menu_open),
      WaitForElementToRender(kNtpElementId, kImageUploadItem),
      CheckJsResultAt(kNtpElementId, kImageUploadItem,
                      "(el) => el.textContent.includes('" +
                          std::string(kInputTypeAddImage) + "')"),
      WaitForElementToRender(kNtpElementId, kFileUploadItem),
      CheckJsResultAt(kNtpElementId, kFileUploadItem,
                      "(el) => el.textContent.includes('" +
                          std::string(kInputTypeAddFile) + "')"),
      WaitForElementToRender(kNtpElementId, kCreateImagesItem),
      CheckJsResultAt(kNtpElementId, kCreateImagesItem,
                      "(el) => el.textContent.includes('" +
                          std::string(kToolCreateImages) + "')"),
      WaitForElementToRender(kNtpElementId, kCanvasItem),
      CheckJsResultAt(kNtpElementId, kCanvasItem,
                      "(el) => el.textContent.includes('" +
                          std::string(kToolCanvas) + "')"),
      WaitForElementToRender(kNtpElementId, kFastModelItem),
      CheckJsResultAt(kNtpElementId, kFastModelItem,
                      "(el) => el.textContent.includes('" +
                          std::string(kModelFastLabel) +
                          "') || "
                          "el.textContent.includes('" +
                          std::string(kModelAutoLabel) + "')"),
      WaitForElementToRender(kNtpElementId, kProModelItem),
      CheckJsResultAt(kNtpElementId, kProModelItem,
                      "(el) => el.textContent.includes('" +
                          std::string(kModelProLabel) + "')"));
}

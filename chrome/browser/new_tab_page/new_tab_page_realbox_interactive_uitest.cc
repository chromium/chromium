// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/check_deref.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/mock_contextual_search_service.h"
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
#include "ui/events/keycodes/keyboard_codes.h"
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
using DeepQuery = InteractiveBrowserWindowTestApi::DeepQuery;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);

static constexpr std::string_view kModelFastLabel = "Fast";
static constexpr std::string_view kModelAutoLabel = "Auto";
static constexpr std::string_view kModelProLabel = "Pro";
static constexpr std::string_view kHintText = "Ask anything";
static constexpr std::string_view kInputTypeAddImage = "Add image";
static constexpr std::string_view kInputTypeAddFile = "Add file";
static constexpr std::string_view kToolCreateImages = "Create images";
static constexpr std::string_view kToolCanvas = "Canvas";

std::string GetModeSelector(omnibox::ToolMode mode) {
  return ".dropdown-item[data-mode='" +
         base::NumberToString(static_cast<int>(mode)) + "']";
}

std::string GetModelSelector(omnibox::ModelMode model) {
  return ".dropdown-item[data-model='" +
         base::NumberToString(static_cast<int>(model)) + "']";
}

const DeepQuery kRealbox = {"ntp-app", "ntp-searchbox", "#inputWrapper"};
const DeepQuery kContextualEntrypoint = {"ntp-app", "ntp-searchbox", "#context",
                                         "#entrypointButton", "#entrypoint"};
const DeepQuery kContextMenuDialog = {"ntp-app", "ntp-searchbox", "#context",
                                      "#menu",   "#menu",         "#dialog"};
const DeepQuery kComposeboxInput = {"ntp-app", "cr-composebox",
                                    "cr-composebox-input", "#input"};
const DeepQuery kComposeboxSubmitButton = {"ntp-app", "#composebox",
                                           "#submitContainer"};
const DeepQuery kComposeboxDialog = {"ntp-app", "#composeboxDialog"};
const DeepQuery kCreateImagesItem = {
    "ntp-app", "ntp-searchbox", "#context", "#menu",
    GetModeSelector(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN)};
const DeepQuery kCanvasItem = {
    "ntp-app", "ntp-searchbox", "#context", "#menu",
    GetModeSelector(omnibox::ToolMode::TOOL_MODE_CANVAS)};
const DeepQuery kToolChipButton = {"ntp-app", "cr-composebox", "#context",
                                   "cr-composebox-tool-chip",
                                   "#toolEnabledButton"};

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

  auto* config = &mock_aim_eligibility_service->config();

  auto* image_input_config = config->add_input_type_configs();
  image_input_config->set_input_type(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
  image_input_config->set_menu_label(std::string(kInputTypeAddImage));

  auto* file_input_config = config->add_input_type_configs();
  file_input_config->set_input_type(omnibox::InputType::INPUT_TYPE_LENS_FILE);
  file_input_config->set_menu_label(std::string(kInputTypeAddFile));

  auto* tab_input_config = config->add_input_type_configs();
  tab_input_config->set_input_type(omnibox::InputType::INPUT_TYPE_BROWSER_TAB);

  auto* canvas_config = config->add_tool_configs();
  canvas_config->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);
  canvas_config->set_menu_label(std::string(kToolCanvas));
  auto* canvas_tool_rule = canvas_config->mutable_rule();
  canvas_tool_rule->set_allow_all_input_types(true);
  canvas_tool_rule->set_allow_all_models(true);

  auto* image_gen_config = config->add_tool_configs();
  image_gen_config->set_tool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  image_gen_config->set_menu_label(std::string(kToolCreateImages));
  auto* image_gen_tool_rule = image_gen_config->mutable_rule();
  image_gen_tool_rule->set_allow_all_input_types(true);
  image_gen_tool_rule->set_allow_all_models(true);

  auto* regular_config = config->add_model_configs();
  regular_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  regular_config->set_menu_label(std::string(kModelFastLabel));

  auto* pro_config = config->add_model_configs();
  pro_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  pro_config->set_menu_label(std::string(kModelProLabel));
  config->set_hint_text(std::string(kHintText));

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
  ON_CALL(*mock_aim_eligibility_service, IsFuseboxEligible())
      .WillByDefault(testing::Return(true));

  return std::move(mock_aim_eligibility_service);
}

std::unique_ptr<KeyedService> BuildMockContextualSearchServiceInstance(
    content::BrowserContext* context) {
  auto mock_service =
      std::make_unique<contextual_search::MockContextualSearchService>(
          /*identity_manager=*/nullptr,
          /*url_loader_factory=*/nullptr,
          /*template_url_service=*/nullptr,
          /*variations_client=*/nullptr, version_info::Channel::UNKNOWN,
          "en-US");

  ON_CALL(*mock_service, CreateSession)
      .WillByDefault(
          [service_ptr = mock_service.get()](
              std::unique_ptr<
                  contextual_search::ContextualSearchContextController::
                      ConfigParams> params,
              contextual_search::ContextualSearchSource source,
              std::optional<lens::LensOverlayInvocationSource>
                  invocation_source) {
            auto query_controller = std::make_unique<MockQueryController>(
                /*identity_manager=*/nullptr, /*url_loader_factory=*/nullptr,
                version_info::Channel::UNKNOWN, "en-US",
                /*template_url_service=*/nullptr,
                /*variations_client=*/nullptr, std::move(params));

            auto* query_controller_ptr = query_controller.get();

            ON_CALL(*query_controller_ptr, StartFileUploadFlow)
                .WillByDefault(
                    [query_controller_ptr](
                        const base::UnguessableToken& file_token,
                        std::unique_ptr<lens::ContextualInputData> input,
                        std::optional<lens::ImageEncodingOptions> options) {
                      query_controller_ptr->NotifySuccess(file_token);
                    });
            ON_CALL(*query_controller_ptr, CreateSearchUrl)
                .WillByDefault(
                    [](std::unique_ptr<
                           MockQueryController::CreateSearchUrlRequestInfo>
                           search_url_request_info,
                       base::OnceCallback<void(GURL)> callback) {
                      std::string query = search_url_request_info->query_text;
                      base::ReplaceChars(query, " ", "+", &query);
                      std::move(callback).Run(
                          GURL("https://www.google.com/search?q=" + query));
                    });

            auto metrics_recorder =
                std::make_unique<MockContextualSearchMetricsRecorder>();

            return service_ptr->CreateSessionForTesting(
                std::move(query_controller), std::move(metrics_recorder));
          });

  return std::move(mock_service);
}

}  // namespace

class NtpRealboxUiTestBase
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  NtpRealboxUiTestBase() = default;
  ~NtpRealboxUiTestBase() override = default;

 protected:
  static std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures(
      std::optional<RealboxLayoutMode> layout_mode = std::nullopt,
      bool compose_button_enabled = true) {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    base::FieldTrialParams realbox_params;
    if (layout_mode.has_value()) {
      realbox_params[ntp_realbox::kRealboxLayoutMode.name] = [=]() {
        switch (layout_mode.value()) {
          case RealboxLayoutMode::kTallBottomContext:
            return ntp_realbox::kRealboxLayoutModeTallBottomContext;
          case RealboxLayoutMode::kTallTopContext:
            return ntp_realbox::kRealboxLayoutModeTallTopContext;
          case RealboxLayoutMode::kCompact:
            return ntp_realbox::kRealboxLayoutModeCompact;
        }
      }();
    }
    enabled_features.emplace_back(ntp_realbox::kNtpRealboxNext, realbox_params);
    enabled_features.emplace_back(omnibox::kAimEnabled,
                                  base::FieldTrialParams());
    enabled_features.emplace_back(omnibox::kAimServerEligibilityEnabled,
                                  base::FieldTrialParams());
    enabled_features.emplace_back(omnibox::kAimUsePecApi,
                                  base::FieldTrialParams());

    if (compose_button_enabled) {
      base::FieldTrialParams composebox_params;
      composebox_params[ntp_composebox::kContextMenuEnableMultiTabSelection
                            .name] = "true";
      enabled_features.emplace_back(ntp_composebox::kNtpComposebox,
                                    composebox_params);
    }

    return enabled_features;
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InteractiveBrowserTest::SetUpBrowserContextKeyedServices(context);
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce(BuildMockAimServiceEligibilityServiceInstance));
    ContextualSearchServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce(BuildMockContextualSearchServiceInstance));
  }
};

class NtpRealboxUiScreenshotTest
    : public NtpRealboxUiTestBase,
      public testing::WithParamInterface<NtpRealboxUiTestParams> {
 public:
  NtpRealboxUiScreenshotTest() = default;
  ~NtpRealboxUiScreenshotTest() override = default;

  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetEnabledFeatures(GetParam().layout_mode,
                           GetParam().compose_button_enabled);

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
    if (!GetParam().compose_button_enabled) {
      disabled_features.push_back(ntp_composebox::kNtpComposebox);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);

    NtpRealboxUiTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    NtpRealboxUiTestBase::SetUpOnMainThread();
    // Sanity check that the NtpRealboxUiTestParams setup actually took; if it
    // didn't, then we can't accurately perform the test.
    ASSERT_EQ(RealboxLayoutModeToString(ntp_realbox::kRealboxLayoutMode.Get()),
              RealboxLayoutModeToString(GetParam().layout_mode));
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    // Bypass NtpRealboxUiTestBase to avoid mocking ContextualSearchService
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
    NtpRealboxUiScreenshotTest,
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

// TODO(crbug.com/454761015): Re-enable after fixing.
IN_PROC_BROWSER_TEST_P(NtpRealboxUiScreenshotTest, DISABLED_Screenshots) {
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
  const DeepQuery kContextMenuEntrypoint = {"ntp-app", "ntp-searchbox",
                                            "#context"};

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

class NtpRealboxInteractiveTest : public NtpRealboxUiTestBase {
 public:
  NtpRealboxInteractiveTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NtpRealboxInteractiveTest, AimButtonOpensComposebox) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kComposeboxDialogOpenEvent);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kComposeboxInputClearedEvent);

  const DeepQuery kComposeButton = {"ntp-app", "ntp-searchbox",
                                    "#composeButton"};

  WebContentsInteractionTestUtil::StateChange composebox_dialog_open;
  composebox_dialog_open.event = kComposeboxDialogOpenEvent;
  composebox_dialog_open.where = kComposeboxDialog;
  composebox_dialog_open.test_function =
      "(el) => el && el.hasAttribute('open')";

  WebContentsInteractionTestUtil::StateChange composebox_input_cleared;
  composebox_input_cleared.event = kComposeboxInputClearedEvent;
  composebox_input_cleared.where = kComposeboxInput;
  composebox_input_cleared.test_function = "(el) => el && el.value === ''";

  RunTestSequence(
      // 1. Open a site.
      AddInstrumentedTab(kFirstTabId, GURL("https://www.google.com")),
      // 2. Load NTP.
      AddInstrumentedTab(kNtpElementId, GURL(chrome::kChromeUINewTabURL)),
      // 3. Assert NTP has loaded by waiting for the realbox and compose button
      // to render.
      WaitForElementToRender(kNtpElementId, kRealbox),
      WaitForElementToRender(kNtpElementId, kComposeButton),
      // 4. Click on the compose button.
      ClickElement(kNtpElementId, kComposeButton),
      // 5. Observe/assert that the composebox dialog is open.
      WaitForStateChange(kNtpElementId, composebox_dialog_open),
      // 6. Insert text into composebox.
      ExecuteJsAt(kNtpElementId, kComposeboxInput,
                  "(el) => { el.value = 'hello'; el.dispatchEvent(new "
                  "Event('input', {bubbles: true, composed: true})); }"),
      // 7. Hit ESC.
      SendKeyPress(kNtpElementId, ui::VKEY_ESCAPE),
      // 8. Wait for composebox input to clear.
      WaitForStateChange(kNtpElementId, composebox_input_cleared),
      // 9. Hit ESC again.
      SendKeyPress(kNtpElementId, ui::VKEY_ESCAPE),
      // 10. Check that composebox dialog has been removed.
      EnsureNotPresent(kNtpElementId, kComposeboxDialog));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxInteractiveTest,
                       ContextualEntrypointMenuHasOptions) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kContextMenuOpenEvent);

  WebContentsInteractionTestUtil::StateChange context_menu_open;
  context_menu_open.event = kContextMenuOpenEvent;
  context_menu_open.where = kContextMenuDialog;
  context_menu_open.test_function = "(el) => el && el.open";

  const DeepQuery kImageUploadItem = {"ntp-app", "ntp-searchbox", "#context",
                                      "#menu", "#imageUpload"};
  const DeepQuery kFileUploadItem = {"ntp-app", "ntp-searchbox", "#context",
                                     "#menu", "#fileUpload"};
  const DeepQuery kFastModelItem = {
      "ntp-app", "ntp-searchbox", "#context", "#menu",
      GetModelSelector(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR)};
  const DeepQuery kProModelItem = {
      "ntp-app", "ntp-searchbox", "#context", "#menu",
      GetModelSelector(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO)};

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

IN_PROC_BROWSER_TEST_F(NtpRealboxInteractiveTest,
                       ContextualEntrypointAttachTabTriggersComposebox) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kContextMenuOpenEvent);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kContextMenuClosedEvent);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSubmitEnabledEvent);

  const DeepQuery kFirstTabItem = {"ntp-app", "ntp-searchbox", "#context",
                                   "#menu", ".dropdown-item[data-index='0']"};
  const DeepQuery kComposeboxFirstTabItem = {
      "ntp-app",  "#composebox",
      "#context", "#contextEntrypoint",
      "#menu",    ".dropdown-item[data-index='0']"};

  WebContentsInteractionTestUtil::StateChange context_menu_open;
  context_menu_open.event = kContextMenuOpenEvent;
  context_menu_open.where = kContextMenuDialog;
  context_menu_open.test_function = "(el) => el && el.open";

  WebContentsInteractionTestUtil::StateChange context_menu_closed;
  context_menu_closed.event = kContextMenuClosedEvent;
  context_menu_closed.where = kContextMenuDialog;
  context_menu_closed.test_function = "(el) => !el || !el.open";

  WebContentsInteractionTestUtil::StateChange submit_enabled;
  submit_enabled.event = kSubmitEnabledEvent;
  submit_enabled.where = kComposeboxSubmitButton;
  submit_enabled.test_function =
      "(el) => el && el.querySelector('#submitIcon') && "
      "!el.querySelector('#submitIcon').hasAttribute('disabled')";

  RunTestSequence(
      // 1. Open a webpage and NTP in separate tabs.
      AddInstrumentedTab(kFirstTabId, GURL("https://www.google.com/")),
      AddInstrumentedTab(kNtpElementId, GURL(chrome::kChromeUINewTabURL)),
      // 2. Assert NTP has loaded by waiting for the Realbox.
      WaitForElementToRender(kNtpElementId, kRealbox),
      // 3. Wait for Contextual Entrypoint Button to render and click it.
      WaitForElementToRender(kNtpElementId, kContextualEntrypoint),
      ClickElement(kNtpElementId, kContextualEntrypoint),
      // 4. Wait for the context menu to open with recent tabs.
      WaitForStateChange(kNtpElementId, context_menu_open),
      WaitForElementToRender(kNtpElementId, kFirstTabItem),
      // 5. Click on First Tab in context menu.
      ClickElement(kNtpElementId, kFirstTabItem),
      // 6. Wait for the tab to load in the composebox context menu.
      WaitForElementToRender(kNtpElementId, kComposeboxFirstTabItem),
      // 7. Hit `ESC` button to dismiss context menu.
      SendKeyPress(kNtpElementId, ui::VKEY_ESCAPE),
      // 8. Wait for context menu to close.
      WaitForStateChange(kNtpElementId, context_menu_closed),
      // 9. After context menu is closed, composeboxdialog remain open.
      CheckJsResultAt(kNtpElementId, kComposeboxDialog,
                      "(el) => el && el.hasAttribute('open')"),
      // 10. Check the placeholder text inside composebox input.
      CheckJsResultAt(
          kNtpElementId, kComposeboxInput,
          "(el) => el.placeholder.includes('" + std::string(kHintText) + "')"),
      // 11. Insert text into composebox.
      ExecuteJsAt(
          kNtpElementId, kComposeboxInput,
          "(el) => { el.value = 'Summarize this page'; el.dispatchEvent(new "
          "Event('input', {bubbles: true, composed: true})); }"),
      // 12. Wait for submit button to be enabled and click it.
      WaitForStateChange(kNtpElementId, submit_enabled),
      ClickElement(kNtpElementId, kComposeboxSubmitButton),
      // 13. Wait for navigation.
      WaitForWebContentsNavigation(kNtpElementId),
      // 14. Ensure tab navigates to a Google search results page.
      CheckResult(
          [this]() {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL()
                .spec();
          },
          testing::StartsWith(
              "https://www.google.com/search?q=Summarize+this+page")));
}

struct NtpRealboxToolInteractiveTestParams {
  DeepQuery tool_context_menu_item;
  DeepQuery tool_chip;
  std::string tool_label;
};

class NtpRealboxToolInteractiveTest
    : public NtpRealboxUiTestBase,
      public testing::WithParamInterface<NtpRealboxToolInteractiveTestParams> {
 public:
  NtpRealboxToolInteractiveTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    NtpRealboxToolInteractiveTest,
    ValuesIn(std::vector<NtpRealboxToolInteractiveTestParams>{
        {
            .tool_context_menu_item = kCanvasItem,
            .tool_chip = kToolChipButton,
            .tool_label = std::string(kToolCanvas),
        },
        {
            .tool_context_menu_item = kCreateImagesItem,
            .tool_chip = kToolChipButton,
            .tool_label = std::string(kToolCreateImages),
        },
    }));

IN_PROC_BROWSER_TEST_P(NtpRealboxToolInteractiveTest,
                       ContextualEntrypointOpenComposeboxWithChip) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kContextMenuOpenEvent);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kToolChipReadyEvent);
  WebContentsInteractionTestUtil::StateChange context_menu_open;
  context_menu_open.event = kContextMenuOpenEvent;
  context_menu_open.where = kContextMenuDialog;
  context_menu_open.test_function = "(el) => el && el.open";
  WebContentsInteractionTestUtil::StateChange tool_chip_ready;
  tool_chip_ready.event = kToolChipReadyEvent;
  tool_chip_ready.where = GetParam().tool_chip;
  tool_chip_ready.test_function =
      "(el) => el && el.textContent.includes('" + GetParam().tool_label + "')";

  RunTestSequence(
      // 1. Open NTP Tab.
      AddInstrumentedTab(kNtpElementId, GURL(chrome::kChromeUINewTabURL)),
      // 2. Wait for Realbox and Contextual Entrypoint Button to render.
      WaitForElementToRender(kNtpElementId, kRealbox),
      WaitForElementToRender(kNtpElementId, kContextualEntrypoint),
      // 3. Click on Contextual Entrypoint Button.
      ClickElement(kNtpElementId, kContextualEntrypoint),
      // 4. Wait for the context menu to open.
      WaitForStateChange(kNtpElementId, context_menu_open),
      // 5. Wait for the tool button to render in context menu.
      WaitForElementToRender(kNtpElementId, GetParam().tool_context_menu_item),
      // 6. Click on tool button in context menu.
      ClickElement(kNtpElementId, GetParam().tool_context_menu_item),
      // 7. Wait for the tool chip to render with the correct text.
      WaitForStateChange(kNtpElementId, tool_chip_ready));
}

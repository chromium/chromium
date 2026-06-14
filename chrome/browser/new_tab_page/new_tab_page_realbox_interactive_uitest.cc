// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/check_deref.h"
#include "base/containers/extend.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_interactive_test_mixin.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/mock_contextual_search_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/shell_dialogs/select_file_dialog.h"

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
using ::testing::ValuesIn;
using DeepQuery = InteractiveBrowserWindowTestApi::DeepQuery;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpElementId);

static constexpr std::string_view kModelFastLabel = "Fast";
static constexpr std::string_view kModelAutoLabel = "Auto";
static constexpr std::string_view kModelProLabel = "Pro";
static constexpr std::string_view kHintText = "Ask anything";
static constexpr std::string_view kInputTypeAddImage = "Add image";
static constexpr std::string_view kInputTypeAddFile = "Add file";
static constexpr std::string_view kToolCreateImages = "Create images";
static constexpr std::string_view kToolCanvas = "Canvas";
static constexpr std::string_view kToolDeepSearch = "Deep search";
// Files used for file upload tests.
static constexpr std::string_view kImageFileName = "handbag.png";
static constexpr std::string_view kPdfFileName = "download.pdf";
// The host used by the Lens service for image uploads.
static constexpr std::string_view kLensSearchURL = "lens.google.com";

std::string GetModeSelector(omnibox::ToolMode mode) {
  return ".dropdown-item[data-mode='" +
         base::NumberToString(static_cast<int>(mode)) + "']";
}

std::string GetModelSelector(omnibox::ModelMode model) {
  return ".dropdown-item[data-model='" +
         base::NumberToString(static_cast<int>(model)) + "']";
}

const DeepQuery kRealbox = {"ntp-app", "ntp-searchbox", "#inputWrapper"};
const DeepQuery kRealboxInput = {"ntp-app", "ntp-searchbox", "#input",
                                 "#input"};
const DeepQuery kRealboxMatch = {"ntp-app", "ntp-searchbox",
                                 "cr-searchbox-dropdown", "cr-searchbox-match",
                                 "#suggestion"};
const DeepQuery kRealboxMatchRemoveButton = {"ntp-app", "ntp-searchbox",
                                             "cr-searchbox-dropdown",
                                             "cr-searchbox-match", "#remove"};
const DeepQuery kVoiceSearchButton = {"ntp-app", "ntp-searchbox",
                                      "#voiceSearchButton"};
const DeepQuery kLensSearchButton = {"ntp-app", "ntp-searchbox",
                                     "#lensSearchButton"};
const DeepQuery kComposeButton = {"ntp-app", "ntp-searchbox", "#composeButton",
                                  "#composeButton"};
const DeepQuery kComposeboxVoiceSearchButton = {"ntp-app", "#composebox",
                                                "#voiceSearchButton"};
const DeepQuery kContextualEntrypoint = {"ntp-app", "ntp-searchbox", "#context",
                                         "#entrypointButton", "#entrypoint"};
const DeepQuery kSearchboxContextMenuDialog = {
    "ntp-app", "ntp-searchbox", "#context", "#menu", "#menu", "#dialog"};
const DeepQuery kComposeboxContextMenuDialog = {
    "ntp-app", "#composebox", "#contextEntrypoint",
    "#menu",   "#menu",       "#dialog"};
const DeepQuery kComposeboxInput = {"ntp-app", "#composebox",
                                    "cr-composebox-input", "#input"};
const DeepQuery kComposeboxVoiceSearch = {"ntp-app", "#composebox",
                                          "#voiceSearch"};
const DeepQuery kComposeboxSubmitButton = {
    "ntp-app", "#composebox", "cr-composebox-submit", "#submitContainer"};
const DeepQuery kComposeboxCancelButton = {
    "ntp-app", "#composebox", "cr-composebox-input", "#cancelIcon"};
const DeepQuery kComposeboxDialog = {"ntp-app", "#composeboxDialog"};
const DeepQuery kCreateImagesItem = {
    "ntp-app", "ntp-searchbox", "#context", "#menu",
    GetModeSelector(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN)};
const DeepQuery kCanvasItem = {
    "ntp-app", "ntp-searchbox", "#context", "#menu",
    GetModeSelector(omnibox::ToolMode::TOOL_MODE_CANVAS)};
const DeepQuery kDeepSearchItem = {
    "ntp-app", "ntp-searchbox", "#context", "#menu",
    GetModeSelector(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH)};
const DeepQuery kComposeboxContextEntrypoint = {
    "ntp-app", "#composebox", "#contextEntrypoint", "#entrypointButton",
    "#entrypoint"};
const DeepQuery kComposeboxCreateImagesItem = {
    "ntp-app", "#composebox", "#contextEntrypoint", "#menu",
    GetModeSelector(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN)};
const DeepQuery kComposeboxDeepSearchItem = {
    "ntp-app", "#composebox", "#contextEntrypoint", "#menu",
    GetModeSelector(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH)};
const DeepQuery kImageUploadItem = {"ntp-app", "ntp-searchbox", "#context",
                                    "#menu", "#imageUpload"};
const DeepQuery kFileUploadItem = {"ntp-app", "ntp-searchbox", "#context",
                                   "#menu", "#fileUpload"};
const DeepQuery kComposeboxFileThumbnail = {"ntp-app", "#composebox",
                                            "cr-composebox-file-carousel",
                                            "cr-composebox-file-thumbnail"};
const DeepQuery kToolChipButton = {"ntp-app", "#composebox", "#context",
                                   "cr-composebox-tool-chip",
                                   "#toolEnabledButton"};
const DeepQuery kScrim = {"ntp-app", "#scrim"};
const DeepQuery kSearchboxDropdown = {"ntp-app", "ntp-searchbox",
                                      "cr-searchbox-dropdown"};
const DeepQuery kNtpLogo = {"ntp-app", "#logo"};
const DeepQuery kLensUploadDialog = {"ntp-app", "#lensUploadDialog", "#dialog"};
const DeepQuery kLensUploadText = {"ntp-app", "#lensUploadDialog",
                                   "#uploadText"};

// Contains variables on which these tests may be parameterized. This approach
// makes it easy to build sets of relevant tests, vs. the brute-force
// testing::Combine() approach.
struct NtpRealboxScreenshotTestParams {
  bool compose_button_enabled = true;
  ui::NativeTheme::PreferredColorScheme color_scheme =
      ui::NativeTheme::PreferredColorScheme::kLight;
  bool rtl = false;

  std::string ToString() const {
    std::ostringstream oss;
    oss << "RealboxNext";
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
  image_gen_tool_rule->add_allowed_input_types(
      omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
  image_gen_tool_rule->add_allowed_input_types(
      omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  image_gen_tool_rule->set_allow_all_models(true);

  auto* deep_search_config = config->add_tool_configs();
  deep_search_config->set_tool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  deep_search_config->set_menu_label(std::string(kToolDeepSearch));
  auto* deep_search_tool_rule = deep_search_config->mutable_rule();
  deep_search_tool_rule->add_allowed_input_types(
      omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  deep_search_tool_rule->set_allow_all_models(true);

  auto* regular_config = config->add_model_configs();
  regular_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  regular_config->set_menu_label(std::string(kModelFastLabel));

  auto* pro_config = config->add_model_configs();
  pro_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  pro_config->set_menu_label(std::string(kModelProLabel));
  config->set_hint_text(std::string(kHintText));

  return std::move(mock_aim_eligibility_service);
}

}  // namespace

class NtpRealboxUiTestBase
    : public SearchboxInteractiveTestMixin<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  NtpRealboxUiTestBase() = default;
  ~NtpRealboxUiTestBase() override = default;

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    SearchboxInteractiveTestMixin<
        WebUiInteractiveTestMixin<InteractiveBrowserTest>>::TearDownOnMainThread();
  }

  MultiStep FocusAndInputText(
      const ui::ElementIdentifier& contents_id,
      const WebContentsInteractionTestUtil::DeepQuery& element) {
    return Steps(ClickElement(contents_id, element),
                 SendKeyPress(contents_id, ui::VKEY_T),
                 SendKeyPress(contents_id, ui::VKEY_E),
                 SendKeyPress(contents_id, ui::VKEY_S),
                 SendKeyPress(contents_id, ui::VKEY_T));
  }

  auto WaitForDialogStateChange(const DeepQuery& where, bool expected_open) {
    return WaitForJsConditionAt(
        kNtpElementId, where,
        expected_open ? "(el) => el && el.open" : "(el) => el && !el.open");
  }

  auto WaitForElementVisibilityChange(const DeepQuery& where,
                                      bool expected_visible) {
    return WaitForJsConditionAt(
        kNtpElementId, where,
        expected_visible ? "(el) => el && !el.hasAttribute('hidden')"
                         : "(el) => el && el.hasAttribute('hidden')");
  }

  auto WaitForSubmitEnabled() {
    return WaitForJsConditionAt(
        kNtpElementId, kComposeboxSubmitButton,
        "(el) => el && el.querySelector('#submitIcon') && "
        "!el.querySelector('#submitIcon').hasAttribute('disabled')");
  }

  auto WaitForElementToNotExist(const DeepQuery& where) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementDoesNotExistEvent);
    WebContentsInteractionTestUtil::StateChange state_change;
    state_change.event = kElementDoesNotExistEvent;
    state_change.where = where;
    state_change.type =
        WebContentsInteractionTestUtil::StateChange::Type::kDoesNotExist;

    return WaitForStateChange(kNtpElementId, state_change);
  }

 protected:
  static std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    return {contextual_tasks::kContextualTasks};
  }

  static std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures(
      bool compose_button_enabled = true) {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    base::FieldTrialParams realbox_params;
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
      public testing::WithParamInterface<NtpRealboxScreenshotTestParams> {
 public:
  NtpRealboxUiScreenshotTest() = default;
  ~NtpRealboxUiScreenshotTest() override = default;

  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetEnabledFeatures(GetParam().compose_button_enabled);

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

    base::Extend(disabled_features, GetDisabledFeatures());
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);

    NtpRealboxUiTestBase::SetUp();
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
    ValuesIn(std::vector<NtpRealboxScreenshotTestParams>{
        // Compact, compose disabled, light mode, LTR
        {
            .compose_button_enabled = false,
        },
        // Compact, compose enabled, light mode, LTR
        {},
        // Compact, compose enabled, dark mode, RTL
        {
            .color_scheme = ui::NativeTheme::PreferredColorScheme::kDark,
            .rtl = true,
        },
    }),
    [](const testing::TestParamInfo<NtpRealboxScreenshotTestParams>& info) {
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
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
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
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NtpRealboxInteractiveTest, ComposeboxTypedSuggestions) {
  const DeepQuery kComposeboxMatch1 = {"ntp-app", "#composebox", "#matches",
                                       "#match1", "#textContainer"};

  RunTestSequence(
      // Load NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      WaitForElementToRender(kNtpElementId, kRealbox),
      WaitForElementToRender(kNtpElementId, kComposeButton),
      // Click on the compose button.
      ClickElement(kNtpElementId, kComposeButton),
      // Observe/assert that the composebox dialog is open.
      WaitForDialogStateChange(kComposeboxDialog, /*expected_open=*/true),
      // Type "t" into composebox input.
      ClickElement(kNtpElementId, kComposeboxInput),
      SendKeyPress(kNtpElementId, ui::VKEY_T),
      // Wait for suggestion to appear.
      WaitForMatch(kNtpElementId, kComposeboxMatch1, "suggestion-1"),
      // Click the match.
      ClickElement(kNtpElementId, kComposeboxMatch1),
      // Ensure google search occurs.
      WaitForGoogleSearch(kNtpElementId, {{"q", "suggestion-1"}}));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxInteractiveTest, RealboxMultilineInputTest) {
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/496928186): Re-enable after de-flaking.
  GTEST_SKIP() << "Flaky on ChromeOS";
#else
  RunTestSequence(
      // Load NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Wait for Realbox to render.
      WaitForElementToRender(kNtpElementId, kRealboxInput),
      // Click on Realbox input.
      ClickElement(kNtpElementId, kRealboxInput),
      // Type 'a' into Realbox input.
      SendKeyPress(kNtpElementId, ui::VKEY_A),
      // Press Shift + Enter to add a newline.
      SendKeyPress(kNtpElementId, ui::VKEY_RETURN, ui::EF_SHIFT_DOWN),
      // Type 'b' into Realbox input.
      SendKeyPress(kNtpElementId, ui::VKEY_B),
      // Wait for Realbox input to have a newline between 'a' and 'b'.
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el && el.value === 'a\\nb'"));
#endif
}

IN_PROC_BROWSER_TEST_F(NtpRealboxInteractiveTest,
                       ContextualEntrypointMenuHasOptions) {
  const DeepQuery kFastModelItem = {
      "ntp-app", "ntp-searchbox", "#context", "#menu",
      GetModelSelector(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR)};
  const DeepQuery kProModelItem = {
      "ntp-app", "ntp-searchbox", "#context", "#menu",
      GetModelSelector(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO)};

  RunTestSequence(
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      WaitForElementToRender(kNtpElementId, kRealbox),
      WaitForElementToRender(kNtpElementId, kContextualEntrypoint),
      ClickElement(kNtpElementId, kContextualEntrypoint),
      WaitForDialogStateChange(kSearchboxContextMenuDialog,
                               /*expected_open=*/true),
      WaitForElementToRender(kNtpElementId, kImageUploadItem),
      CheckJsResultAt(kNtpElementId, kImageUploadItem,
                      "(el) => el.textContent.trim() === '" +
                          std::string(kInputTypeAddImage) + "'"),
      WaitForElementToRender(kNtpElementId, kFileUploadItem),
      CheckJsResultAt(kNtpElementId, kFileUploadItem,
                      "(el) => el.textContent.trim() === '" +
                          std::string(kInputTypeAddFile) + "'"),
      WaitForElementToRender(kNtpElementId, kCreateImagesItem),
      CheckJsResultAt(kNtpElementId, kCreateImagesItem,
                      "(el) => el.textContent.trim() === '" +
                          std::string(kToolCreateImages) + "'"),
      WaitForElementToRender(kNtpElementId, kCanvasItem),
      CheckJsResultAt(kNtpElementId, kCanvasItem,
                      "(el) => el.textContent.trim() === '" +
                          std::string(kToolCanvas) + "'"),
      WaitForElementToRender(kNtpElementId, kDeepSearchItem),
      CheckJsResultAt(kNtpElementId, kDeepSearchItem,
                      "(el) => el.textContent.trim() === '" +
                          std::string(kToolDeepSearch) + "'"),
      WaitForElementToRender(kNtpElementId, kFastModelItem),
      CheckJsResultAt(kNtpElementId, kFastModelItem,
                      "(el) => el.textContent.trim() === '" +
                          std::string(kModelFastLabel) +
                          "' || "
                          "el.textContent.trim() === '" +
                          std::string(kModelAutoLabel) + "'"),
      WaitForElementToRender(kNtpElementId, kProModelItem),
      CheckJsResultAt(kNtpElementId, kProModelItem,
                      "(el) => el.textContent.trim() === '" +
                          std::string(kModelProLabel) + "'"));
}

struct NtpRealboxUploadInteractiveTestParams {
  DeepQuery upload_context_menu_item;
  std::string file_name;
};

class NtpRealboxUploadInteractiveTest
    : public NtpRealboxUiTestBase,
      public testing::WithParamInterface<
          NtpRealboxUploadInteractiveTestParams> {
 public:
  NtpRealboxUploadInteractiveTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NtpRealboxUploadInteractiveTest,
    ValuesIn(std::vector<NtpRealboxUploadInteractiveTestParams>{
        {
            .upload_context_menu_item = kImageUploadItem,
            .file_name = std::string(kImageFileName),
        },
        {
            .upload_context_menu_item = kFileUploadItem,
            .file_name = std::string(kPdfFileName),
        },
    }));

IN_PROC_BROWSER_TEST_P(NtpRealboxUploadInteractiveTest,
                       ContextualEntrypointUploadTriggersComposebox) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII(GetParam().file_name);

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  RunTestSequence(
      // Open NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Assert NTP has loaded by waiting for the Realbox.
      WaitForElementToRender(kNtpElementId, kRealbox),
      // Wait for Contextual Entrypoint Button to render and click it.
      WaitForElementToRender(kNtpElementId, kContextualEntrypoint),
      ClickElement(kNtpElementId, kContextualEntrypoint),
      // Wait for the context menu to open.
      WaitForDialogStateChange(kSearchboxContextMenuDialog,
                               /*expected_open=*/true),
      WaitForElementToRender(kNtpElementId,
                             GetParam().upload_context_menu_item),
      // Click on Upload item in context menu.
      ClickElement(kNtpElementId, GetParam().upload_context_menu_item),
      // Wait for searchbox context menu to close and composebox to open.
      WaitForDialogStateChange(kSearchboxContextMenuDialog,
                               /*expected_open=*/false),
      // Wait for the file thumbnail to render in the composebox.
      WaitForElementToRender(kNtpElementId, kComposeboxFileThumbnail),
      // Open the composebox context menu to verify disabled states.
      ClickElement(kNtpElementId, kComposeboxContextEntrypoint),
      WaitForDialogStateChange(kComposeboxContextMenuDialog,
                               /*expected_open=*/true),
      // Check disabled states based on the uploaded file type.
      GetParam().file_name == kImageFileName
          // For image upload, Deep Search should be disabled.
          ? Steps(WaitForElementToRender(kNtpElementId,
                                         kComposeboxDeepSearchItem),
                  CheckJsResultAt(kNtpElementId, kComposeboxDeepSearchItem,
                                  "(el) => el.hasAttribute('disabled')"))
          // For file upload, both Deep Search and Create Images should be
          // disabled.
          : Steps(WaitForElementToRender(kNtpElementId,
                                         kComposeboxDeepSearchItem),
                  CheckJsResultAt(kNtpElementId, kComposeboxDeepSearchItem,
                                  "(el) => el.hasAttribute('disabled')"),
                  WaitForElementToRender(kNtpElementId,
                                         kComposeboxCreateImagesItem),
                  CheckJsResultAt(kNtpElementId, kComposeboxCreateImagesItem,
                                  "(el) => el.hasAttribute('disabled')")),
      // Dismiss the composebox context menu.
      SendKeyPress(kNtpElementId, ui::VKEY_ESCAPE),
      WaitForDialogStateChange(kComposeboxContextMenuDialog,
                               /*expected_open=*/false),
      // Focus composebox input and type something.
      FocusAndInputText(kNtpElementId, kComposeboxInput),
      // Wait for submit button to be enabled and click it.
      WaitForSubmitEnabled(),
      ClickElement(kNtpElementId, kComposeboxSubmitButton),
      // Ensure google search occurs.
      WaitForGoogleSearch(kNtpElementId, {{"q", "test"}}));
}

class NtpRealboxSubmitInteractiveTest
    : public NtpRealboxInteractiveTest,
      public testing::WithParamInterface<bool> {
 public:
  bool ClickMatch() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All, NtpRealboxSubmitInteractiveTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(NtpRealboxSubmitInteractiveTest,
                       SubmittingInputNavigatesToSearch) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!ClickMatch()) {
    // TODO(crbug.com/496928186): Re-enable after de-flaking.
    GTEST_SKIP() << "Flaky on ChromeOS";
  }
#endif

  RunTestSequence(
      // Wait for the realbox to render on the NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      WaitForElementToRender(kNtpElementId, kRealbox),
      // Seed at least two history results to ensure the dropdown is visible in
      // multiline mode and to prevent flakiness.
      // The most recently seeded result appears first in the dropdown.
      SeedSearchboxResult("aimode"), SeedSearchboxResult("a"),
      // Type into the realbox and wait for a verbatim match to appear.
      ClickElement(kNtpElementId, kRealboxInput),
      SendKeyPress(kNtpElementId, ui::VKEY_A),
      WaitForVerbatimMatch(kNtpElementId, kRealboxMatch, "a"),
      // Click the match or press enter depending on the test parameter.
      // JS `KeyboardEvent` is used instead of `SendKeyPress` to avoid flakiness
      // caused by OS-level focus races and IME composition bugs.
      If([&]() { return ClickMatch(); },
         Then(ClickElement(kNtpElementId, kRealboxMatch)),
         Else(ExecuteJsAt(
             kNtpElementId, kRealboxInput,
             "(el) => { el.dispatchEvent(new KeyboardEvent('keydown', { "
             "key:'Enter', bubbles: true, cancelable: true, composed: true "
             "})); }"))),
      // Ensure google search occurs.
      WaitForGoogleSearch(kNtpElementId, {{"q", "a"}}));
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
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
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
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kToolChipReadyEvent);
  WebContentsInteractionTestUtil::StateChange tool_chip_ready;
  tool_chip_ready.event = kToolChipReadyEvent;
  tool_chip_ready.where = GetParam().tool_chip;
  tool_chip_ready.test_function =
      "(el) => el && el.textContent.trim() === '" + GetParam().tool_label + "'";

  RunTestSequence(
      // 1. Open NTP Tab.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // 2. Wait for Realbox and Contextual Entrypoint Button to render.
      WaitForElementToRender(kNtpElementId, kRealbox),
      WaitForElementToRender(kNtpElementId, kContextualEntrypoint),
      // 3. Click on Contextual Entrypoint Button.
      ClickElement(kNtpElementId, kContextualEntrypoint),
      // 4. Wait for the context menu to open.
      WaitForDialogStateChange(kSearchboxContextMenuDialog,
                               /*expected_open=*/true),
      // 5. Wait for the tool button to render in context menu.
      WaitForElementToRender(kNtpElementId, GetParam().tool_context_menu_item),
      // 6. Click on tool button in context menu.
      ClickElement(kNtpElementId, GetParam().tool_context_menu_item),
      // 7. Wait for the tool chip to render with the correct text.
      WaitForStateChange(kNtpElementId, tool_chip_ready));
}

struct ComposeboxSearchParam {
  bool is_voice = false;
  bool submit_via_keyboard = false;
};

class NtpComposeboxSearchFulfillmentTest
    : public NtpRealboxUiTestBase,
      public testing::WithParamInterface<ComposeboxSearchParam> {
 public:
  NtpComposeboxSearchFulfillmentTest() {
    content::SpeechRecognitionManager::SetManagerForTesting(
        &fake_speech_recognition_manager_);
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    NtpRealboxUiTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch("use-fake-ui-for-media-stream");
  }

 protected:
  content::FakeSpeechRecognitionManager fake_speech_recognition_manager_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NtpComposeboxSearchFulfillmentTest,
    testing::Values(ComposeboxSearchParam{},
                    ComposeboxSearchParam{.submit_via_keyboard = true},
                    ComposeboxSearchParam{.is_voice = true}),
    [](const testing::TestParamInfo<ComposeboxSearchParam>& info) {
      return base::StringPrintf(
          "%s%s", info.param.is_voice ? "Voice" : "Typed",
          info.param.submit_via_keyboard ? "Keyboard" : "Click");
    });

IN_PROC_BROWSER_TEST_P(NtpComposeboxSearchFulfillmentTest,
                       SearchNavigatesOnSubmit) {
  const ComposeboxSearchParam& param = GetParam();
  const std::string query = "test";

  if (param.is_voice) {
    fake_speech_recognition_manager_.set_should_send_fake_response(false);
    fake_speech_recognition_manager_.SetFakeResult(query, /*is_final=*/true);
  }

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kVoiceSearchVisibleEvent);
  WebContentsInteractionTestUtil::StateChange voice_search_visible;
  voice_search_visible.event = kVoiceSearchVisibleEvent;
  voice_search_visible.where = kComposeboxVoiceSearch;
  voice_search_visible.test_function =
      "(el) => el && window.getComputedStyle(el).display !== 'none'";

  RunTestSequence(
      // Load NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Assert NTP has loaded by waiting for the realbox and compose button
      // to render.
      WaitForElementToRender(kNtpElementId, kRealbox),
      WaitForElementToRender(kNtpElementId, kComposeButton),
      // Click on the compose button.
      ClickElement(kNtpElementId, kComposeButton),
      // Observe/assert that the composebox dialog is open.
      WaitForDialogStateChange(kComposeboxDialog, /*expected_open=*/true),

      // Write something into the input field or use voice search.
      param.is_voice
          ? Steps(ClickElement(kNtpElementId, kComposeboxVoiceSearchButton),
                  WaitForStateChange(kNtpElementId, voice_search_visible),
                  Do([&]() {
                    fake_speech_recognition_manager_.SendFakeResponse(
                        /*end_recognition=*/true, base::DoNothing());
                  }),
                  TriggerAimVoiceSearch(kNtpElementId, kComposeboxVoiceSearch,
                                        query))
          : Steps(FocusAndInputText(kNtpElementId, kComposeboxInput),
                  WaitForSubmitEnabled(),
                  param.submit_via_keyboard
                      ? Steps(SendKeyPress(kNtpElementId, ui::VKEY_RETURN))
                      : Steps(ClickElement(kNtpElementId,
                                           kComposeboxSubmitButton))),

      // Ensure tab navigates to a Google search results page.
      WaitForGoogleSearch(kNtpElementId, {{"q", query}}));
}

class NtpComposeboxDismissTest : public NtpRealboxUiTestBase,
                                 public testing::WithParamInterface<bool> {
 public:
  NtpComposeboxDismissTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
  }

  bool CloseViaEscButton() const { return GetParam(); }

  auto WaitForComposeboxInputCleared() {
    return WaitForJsConditionAt(kNtpElementId, kComposeboxInput,
                                "(el) => el && el.value === ''");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(, NtpComposeboxDismissTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(NtpComposeboxDismissTest,
                       ClearsInputAndClosesComposebox) {
  auto TriggerDismissAction = [this]() {
    if (CloseViaEscButton()) {
      return Steps(SendKeyPress(kNtpElementId, ui::VKEY_ESCAPE));
    }
    return Steps(ClickElement(kNtpElementId, kComposeboxCancelButton));
  };

  RunTestSequence(
      // Load NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Assert NTP has loaded by waiting for the realbox and compose button
      // to render.
      WaitForElementToRender(kNtpElementId, kRealbox),
      WaitForElementToRender(kNtpElementId, kComposeButton),
      // Click on the compose button.
      ClickElement(kNtpElementId, kComposeButton),
      // Observe/assert that the composebox dialog is open.
      WaitForDialogStateChange(kComposeboxDialog, /*expected_open=*/true),
      // Ensure the context menu dialog is closed.
      CheckJsResultAt(kNtpElementId, kComposeboxContextMenuDialog,
                      "(el) => el && !el.open"),
      // Check the placeholder text inside composebox input.
      CheckJsResultAt(kNtpElementId, kComposeboxInput,
                      "(el) => el.placeholder && el.placeholder.trim() === '" +
                          std::string(kHintText) + "'"),
      // Focus composebox input and type something.
      FocusAndInputText(kNtpElementId, kComposeboxInput),
      // First dismiss action clears the input.
      TriggerDismissAction(),
      // Wait for composebox input to clear.
      WaitForComposeboxInputCleared(),
      // Second dismiss action closes the dialog.
      TriggerDismissAction(),
      // Check that composebox dialog has been removed.
      WaitForElementToNotExist(kComposeboxDialog));
}

class NtpRealboxCyclingPlaceholderInteractiveTest
    : public NtpRealboxUiTestBase {
 public:
  NtpRealboxCyclingPlaceholderInteractiveTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetEnabledFeatures();
    enabled_features.emplace_back(ntp_realbox::kNtpRealboxCyclingPlaceholders,
                                  base::FieldTrialParams());
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                GetDisabledFeatures());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NtpRealboxCyclingPlaceholderInteractiveTest,
                       PlaceholderCycles) {
  RunTestSequence(
      // Load NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Wait for Realbox to render.
      WaitForElementToRender(kNtpElementId, kRealboxInput),
      // Wait and verify if placeholder text cycles.
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el && el.getAnimations().length > 0"));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxInteractiveTest,
                       ScrimAndDropdownAppearAndDisappear) {
  RunTestSequence(
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      WaitForElementToRender(kNtpElementId, kRealboxInput),
      // Seed history results to ensure the dropdown is populated when typing in
      // to the realbox.
      SeedSearchboxResult("chrome"),
      // Verify that scrim is initially hidden.
      CheckJsResultAt(kNtpElementId, kScrim,
                      "(el) => el && el.hasAttribute('hidden')"),
      // Click realbox input to focus it and trigger the dropdown/scrim.
      ClickElement(kNtpElementId, kRealboxInput),
      // Verify scrim is shown.
      WaitForElementVisibilityChange(kScrim, /*expected_visible=*/true),
      // Verify dropdown is shown.
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/true),
      // Wait for the verbatim match to actually render, guaranteeing
      // suggestions are visible before we click away.
      WaitForVerbatimMatch(kNtpElementId, kRealboxMatch, "chrome"),
      // Click outside to dismiss. The scrim itself covers everything, so
      // clicking it works. We click the logo to ensure we don't accidentally
      // click the dropdown or searchbox which are centered.
      MoveMouseTo(kNtpElementId, kNtpLogo), ClickMouse(),
      // Verify scrim is hidden.
      WaitForElementVisibilityChange(kScrim, /*expected_visible=*/false),
      // Verify dropdown is hidden.
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/false));
}

class NtpRealboxDefaultExperienceInteractiveTest : public NtpRealboxUiTestBase {
 public:
  NtpRealboxDefaultExperienceInteractiveTest() {
    content::SpeechRecognitionManager::SetManagerForTesting(
        &fake_speech_recognition_manager_);
    std::vector<base::test::FeatureRef> disabled_features =
        GetDisabledFeatures();
    for (const auto& feature_ref_and_params : GetEnabledFeatures()) {
      disabled_features.push_back(*feature_ref_and_params.feature);
    }
    disabled_features.push_back(ntp_features::kNtpNextFeatures);

    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::kOmniboxAppendInvocationSource, {}}}, disabled_features);
  }

 protected:
  content::FakeSpeechRecognitionManager fake_speech_recognition_manager_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       DefaultExperienceRealboxUI) {
  RunTestSequence(
      // Load NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Wait for Realbox to render.
      WaitForElementToRender(kNtpElementId, kRealbox),
      // Wait for Voice Search, Lens, and AI Mode buttons to render.
      WaitForElementToRender(kNtpElementId, kVoiceSearchButton),
      WaitForElementToRender(kNtpElementId, kLensSearchButton),
      WaitForElementToRender(kNtpElementId, kComposeButton),
      // Verify the placeholder text is steady.
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el && el.getAnimations().length === 0"),
      // Click into the Searchbox.
      ClickElement(kNtpElementId, kRealboxInput),
      // Verify that the placeholder text disappears.
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el && window.getComputedStyle(el, "
                           "'::placeholder').visibility === 'hidden'"),
      // Type text into Realbox and click AIM Button.
      SendKeyPress(kNtpElementId, ui::VKEY_T),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el && el.value === 't'"),
      ClickElement(kNtpElementId, kComposeButton),
      // Wait for the page to navigate to Google SRP.
      WaitForGoogleSearch(kNtpElementId, {{"q", "t"}, {"udm", "50"}}));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       ClickOutsideAndEscapeBehavior) {
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/496928186): Re-enable after de-flaking.
  GTEST_SKIP() << "Flaky on ChromeOS";
#else
  RunTestSequence(
      // Load NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Wait for Realbox to render.
      WaitForElementToRender(kNtpElementId, kRealboxInput),
      // Wait for Voice Search, Lens, and AI Mode buttons to render.
      WaitForElementToRender(kNtpElementId, kVoiceSearchButton),
      WaitForElementToRender(kNtpElementId, kLensSearchButton),
      WaitForElementToRender(kNtpElementId, kComposeButton),
      // Seed history results to ensure the dropdown is populated.
      SeedSearchboxResult("a"),
      // Click on Realbox.
      ClickElement(kNtpElementId, kRealboxInput),
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/true),
      // Verify: Only AIM Button is Visible in searchbox
      WaitForElementVisibilityChange(kComposeButton, /*expected_visible=*/true),
      WaitForElementToNotExist(kVoiceSearchButton),
      WaitForElementToNotExist(kLensSearchButton),
      // Type Something into Realbox.
      SendKeyPress(kNtpElementId, ui::VKEY_A),
      // Wait for the verbatim match to render.
      WaitForVerbatimMatch(kNtpElementId, kRealboxMatch, "a"),
      // Verify: Clicking outside the realbox should close suggestions dropdown,
      // but leave the text inside the realbox visible.
      MoveMouseTo(kNtpElementId, kNtpLogo), ClickMouse(),
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/false),
      CheckJsResultAt(kNtpElementId, kRealboxInput,
                      "(el) => el && el.value === 'a'"),
      // Check focus is lost
      CheckJsResultAt(kNtpElementId, kRealboxInput,
                      "(el) => !el.matches(':focus')"),
      // Click on realbox and press ‘ESC’ button
      ClickElement(kNtpElementId, kRealboxInput),
      SendKeyPress(kNtpElementId, ui::VKEY_ESCAPE),
      // Verify: Text inside realbox is cleared, but the focus remains.
      // Also verify that the dropdown is closed.
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/false),
      CheckJsResultAt(kNtpElementId, kRealboxInput,
                      "(el) => el && el.value === ''"),
      CheckJsResultAt(kNtpElementId, kRealboxInput,
                      "(el) => el.matches(':focus')"),
      // Verify: Realbox contains AIM Button, Voice Search button, Lens button
      WaitForElementVisibilityChange(kComposeButton, /*expected_visible=*/true),
      WaitForElementToRender(kNtpElementId, kVoiceSearchButton),
      WaitForElementToRender(kNtpElementId, kLensSearchButton));
#endif
}

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       VoiceSearchNavigatesToGoogleSearch) {
  const std::string query = "testing";
  const DeepQuery kVoiceSearchOverlayDialog = {
      "ntp-app", "ntp-voice-search-overlay", "#dialog"};

  // Configure the mock to pause before sending a response so we can verify
  // the overlay UI.
  fake_speech_recognition_manager_.set_should_send_fake_response(false);
  fake_speech_recognition_manager_.SetFakeResult(query, /*is_final=*/true);

  RunTestSequence(
      // Load NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Wait for Realbox to render.
      WaitForElementToRender(kNtpElementId, kRealbox),
      // Wait for Voice Search button to render.
      WaitForElementToRender(kNtpElementId, kVoiceSearchButton),
      // Click on Voice Search button.
      ClickElement(kNtpElementId, kVoiceSearchButton),
      // Verify that the voice search overlay dialog appears and is open.
      WaitForElementToRender(kNtpElementId, kVoiceSearchOverlayDialog),
      WaitForDialogStateChange(kVoiceSearchOverlayDialog,
                               /*expected_open=*/true),
      // Send the mock response.
      Do([&]() {
        fake_speech_recognition_manager_.SendFakeResponse(
            /*end_recognition=*/true,
            /*on_fake_response_sent=*/base::DoNothing());
      }),
      // Wait for the page to navigate to Google SRP.
      WaitForGoogleSearch(kNtpElementId, {{"q", query}}));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       LensImageUploadOpensSRP) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII(kImageFileName);

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  RunTestSequence(
      // Open the New Tab Page (NTP).
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Wait for the Realbox and Lens search button to render on the page.
      WaitForElementToRender(kNtpElementId, kRealbox),
      WaitForElementToRender(kNtpElementId, kLensSearchButton),
      // Click on the Lens search button.
      ClickElement(kNtpElementId, kLensSearchButton),
      // Wait for the Lens upload dialog to become visible.
      WaitForElementVisibilityChange(kLensUploadDialog,
                                     /*expected_visible=*/true),
      // Wait for the clickable "upload" text area within the dialog to render.
      WaitForElementToRender(kNtpElementId, kLensUploadText),
      // Simulate a user clicking the "upload" text to trigger the file picker.
      ClickElement(kNtpElementId, kLensUploadText),
      // Wait for the browser to navigate away from the NTP as a result of the
      // upload.
      WaitForWebContentsNavigation(kNtpElementId),
      // Verify page navigation to the Lens URL.
      CheckElement(
          kNtpElementId,
          [](ui::TrackedElement* el) {
            return el->AsA<TrackedElementWebContents>()
                ->owner()
                ->web_contents()
                ->GetLastCommittedURL()
                .host();
          },
          // Assert that the extracted host matches the expected Lens service
          // host.
          std::string(kLensSearchURL)));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       KeyboardNavigationAndIndexCycling) {
  RunTestSequence(
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      WaitForElementToRender(kNtpElementId, kRealboxInput),
      // Seed history results to ensure the dropdown is populated.
      SeedSearchboxResult("h"),
      // Click realbox input to focus it and trigger the dropdown/scrim.
      ClickElement(kNtpElementId, kRealboxInput),
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/true),
      SendKeyPress(kNtpElementId, ui::VKEY_H),
      // Wait for the verbatim match to render.
      WaitForVerbatimMatch(kNtpElementId, kRealboxMatch, "h"),
      // Press DOWN to select the first suggestion.
      SendKeyPress(kNtpElementId, ui::VKEY_DOWN),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'suggestion-1'"),
      // Press DOWN to select the next suggestion.
      SendKeyPress(kNtpElementId, ui::VKEY_DOWN),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'suggestion-2'"),
      // Press DOWN to wrap around to the first item (which is the verbatim "").
      SendKeyPress(kNtpElementId, ui::VKEY_DOWN),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'h'"),
      // Press UP to wrap around to the bottom item.
      SendKeyPress(kNtpElementId, ui::VKEY_UP),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'suggestion-2'"),
      // Press ENTER to navigate.
      SendKeyPress(kNtpElementId, ui::VKEY_RETURN),
      // Ensure google search occurs.
      WaitForGoogleSearch(kNtpElementId,
                          {{"q", "suggestion-2"}, {"source", "chrome.rb"}}));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       RemoveSuggestionViaClick) {
  RunTestSequence(
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      WaitForElementToRender(kNtpElementId, kRealboxInput),
      // Seed history results to populate the dropdown
      SeedSearchboxResult("aimode"),
      // Click on Realbox to show the dropdown
      ClickElement(kNtpElementId, kRealboxInput),
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/true),
      SendKeyPress(kNtpElementId, ui::VKEY_A),
      // Wait for the inline autocompleted verbatim match to render.
      WaitForVerbatimMatch(kNtpElementId, kRealboxMatch, "aimode"),
      // Wait for the remove button to render and become visible
      WaitForElementToRender(kNtpElementId, kRealboxMatchRemoveButton),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'aimode'"),
      // Click the remove button
      ClickElement(kNtpElementId, kRealboxMatchRemoveButton),
      // After removing the inline autocomplete match, the input should revert
      // to "a"
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'a'"));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       RemoveSuggestionViaKeyboard) {
  RunTestSequence(
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      WaitForElementToRender(kNtpElementId, kRealboxInput),
      // Seed history result to populate the dropdown
      SeedSearchboxResult("a"), SeedSearchboxResult("b"),
      // Click on Realbox to show the dropdown
      ClickElement(kNtpElementId, kRealboxInput),
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/true),
      // Pressing Tab should focus the AIM button.
      SendKeyPress(kNtpElementId, ui::VKEY_TAB),
      WaitForJsConditionAt(kNtpElementId, kComposeButton,
                           "(el) => el && el.matches(':focus')"),
      // Pressing Tab again should focus the inline autocomplete match.
      SendKeyPress(kNtpElementId, ui::VKEY_TAB),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'b'"),
      // Pressing Tab again should focus the remove button.
      SendKeyPress(kNtpElementId, ui::VKEY_TAB),
      WaitForJsConditionAt(kNtpElementId, kRealboxMatchRemoveButton,
                           "(el) => el && el.matches(':focus')"),
      // Trigger the remove button via ENTER
      SendKeyPress(kNtpElementId, ui::VKEY_RETURN),
      // After removing the current match, the next match remove button should
      // be focused.
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'a'"),
      SendKeyPress(kNtpElementId, ui::VKEY_RETURN),
      // After all matches are removed, the input should be empty.
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === ''"));
}

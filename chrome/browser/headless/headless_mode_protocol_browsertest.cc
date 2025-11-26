// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_protocol_browsertest.h"

#include <string_view>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/browser/headless/test/headless_browser_test_utils.h"
#include "components/headless/select_file_dialog/headless_select_file_dialog.h"
#include "components/headless/test/shared_test_util.h"
#include "content/public/common/content_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::NotNull;

namespace headless {

namespace switches {
static const char kResetResults[] = "reset-results";
static const char kDumpDevToolsProtocol[] = "dump-devtools-protocol";
}  // namespace switches

namespace {
static const base::FilePath kTestDataDir(
    FILE_PATH_LITERAL("chrome/browser/headless/test/data"));
static const base::FilePath kSharedTestDataDir(
    FILE_PATH_LITERAL("components/headless/test/data"));

constexpr char kProtocolTestDir[] = "protocol";
}  // namespace

HeadlessModeProtocolBrowserTest::HeadlessModeProtocolBrowserTest() = default;
HeadlessModeProtocolBrowserTest::~HeadlessModeProtocolBrowserTest() = default;

base::FilePath HeadlessModeProtocolBrowserTest::GetTestDataDir() {
  return IsSharedTestScript() ? kSharedTestDataDir : kTestDataDir;
}

base::FilePath HeadlessModeProtocolBrowserTest::GetScriptPath() {
  base::FilePath src_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
  return src_dir.Append(GetTestDataDir())
      .AppendASCII(kProtocolTestDir)
      .AppendASCII(GetScriptName());
}

base::FilePath HeadlessModeProtocolBrowserTest::GetTestExpectationFilePath() {
  return headless::GetTestExpectationFilePath(GetScriptPath(), test_meta_info_,
                                              HeadlessType::kHeadlessMode);
}

bool HeadlessModeProtocolBrowserTest::IsSharedTestScript() {
  return headless::IsSharedTestScript(GetScriptName());
}

void HeadlessModeProtocolBrowserTest::SetUp() {
  LoadTestMetaInfo();
  HeadlessModeDevTooledBrowserTest::SetUp();
}

void HeadlessModeProtocolBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(::network::switches::kHostResolverRules,
                                  "MAP *.test 127.0.0.1");
  HeadlessModeDevTooledBrowserTest::SetUpCommandLine(command_line);

  test_meta_info_.AppendToCommandLine(*command_line);
}

base::Value::Dict HeadlessModeProtocolBrowserTest::GetPageUrlExtraParams() {
  return base::Value::Dict();
}

void HeadlessModeProtocolBrowserTest::LoadTestMetaInfo() {
  base::FilePath script_path = GetScriptPath();
  std::string script_body;
  CHECK(base::ReadFileToString(script_path, &script_body))
      << "script_path=" << script_path;

  auto test_meta_info = TestMetaInfo::FromString(script_body);
  CHECK(test_meta_info.has_value()) << test_meta_info.error();

  test_meta_info_ = test_meta_info.value();
}

void HeadlessModeProtocolBrowserTest::StartEmbeddedTestServer() {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "third_party/blink/web_tests/http/tests/inspector-protocol");

  if (IsSharedTestScript()) {
    embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataDir());
  }

  CHECK(embedded_test_server()->Start());
}

void HeadlessModeProtocolBrowserTest::RunDevTooledTest() {
  StartEmbeddedTestServer();

  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(web_contents_.get());

  // Set up Page domain.
  devtools_client_.AddEventHandler(
      "Page.loadEventFired",
      base::BindRepeating(&HeadlessModeProtocolBrowserTest::OnLoadEventFired,
                          base::Unretained(this)));
  devtools_client_.SendCommand("Page.enable");

  // Expose DevTools protocol to the target.
  browser_devtools_client_.SendCommand(
      "Target.exposeDevToolsProtocol", Param("targetId", agent_host->GetId()),
      base::BindOnce(
          &HeadlessModeProtocolBrowserTest::OnDevToolsProtocolExposed,
          base::Unretained(this)));
}

void HeadlessModeProtocolBrowserTest::OnDevToolsProtocolExposed(
    base::Value::Dict params) {
  // Navigate to test harness page
  GURL page_url = embedded_test_server()->GetURL(
      "harness.test", "/protocol/inspector-protocol-test.html");
  devtools_client_.SendCommand("Page.navigate", Param("url", page_url.spec()));
}

void HeadlessModeProtocolBrowserTest::OnLoadEventFired(
    const base::Value::Dict& params) {
  std::string script_name = GetScriptName();
  GURL test_url = embedded_test_server()->GetURL("harness.test",
                                                 "/protocol/" + script_name);
  GURL target_url =
      embedded_test_server()->GetURL("127.0.0.1", "/protocol/" + script_name);

  base::Value::Dict test_params;
  test_params.Set("test", test_url.spec());
  test_params.Set("target", target_url.spec());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDumpDevToolsProtocol)) {
    test_params.Set("dumpDevToolsProtocol", true);
  }
  test_params.Merge(GetPageUrlExtraParams());

  std::string json_test_params = base::WriteJson(test_params).value_or("");
  std::string evaluate_script = "runTest(" + json_test_params + ")";

  base::Value::Dict evaluate_params;
  evaluate_params.Set("expression", evaluate_script);
  evaluate_params.Set("awaitPromise", true);
  evaluate_params.Set("returnByValue", true);
  devtools_client_.SendCommand(
      "Runtime.evaluate", std::move(evaluate_params),
      base::BindOnce(&HeadlessModeProtocolBrowserTest::OnEvaluateResult,
                     base::Unretained(this)));
}

void HeadlessModeProtocolBrowserTest::OnEvaluateResult(
    base::Value::Dict params) {
  std::string* value = params.FindStringByDottedPath("result.result.value");
  EXPECT_THAT(value, NotNull());

  ProcessTestResult(*value);

  FinishAsyncTest();
}

void HeadlessModeProtocolBrowserTest::ProcessTestResult(
    const std::string& test_result) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath expectation_path = GetTestExpectationFilePath();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kResetResults)) {
    LOG(INFO) << "Updating expectations in " << expectation_path;
    bool success = base::WriteFile(expectation_path, test_result);
    CHECK(success);
  }

  std::string expectation;
  if (!base::ReadFileToString(expectation_path, &expectation)) {
    ADD_FAILURE() << "Unable to read expectations in " << expectation_path
                  << ", run test with --" << switches::kResetResults
                  << " to create expectations.";
    FinishAsyncTest();
    return;
  }

  EXPECT_EQ(expectation, test_result);
}

HEADLESS_MODE_PROTOCOL_TEST(DomFocus, "input/dom-focus.js")
HEADLESS_MODE_PROTOCOL_TEST(FocusEvent, "input/focus-event.js")

// Flaky crbug/1431857
HEADLESS_MODE_PROTOCOL_TEST(DISABLED_FocusBlurNotifications,
                            "input/focus-blur-notifications.js")

// TODO(crbug.com/40257054): Re-enable this test
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_InputClipboardOps DISABLED_InputClipboardOps
#else
#define MAYBE_InputClipboardOps InputClipboardOps
#endif
HEADLESS_MODE_PROTOCOL_TEST(MAYBE_InputClipboardOps,
                            "input/input-clipboard-ops.js")

HEADLESS_MODE_PROTOCOL_TEST(DocumentFocusOnLoad,
                            "input/document-focus-on-load.js")

class HeadlessModeInputSelectFileDialogTest
    : public HeadlessModeProtocolBrowserTest {
 public:
  HeadlessModeInputSelectFileDialogTest() = default;

  void SetUpOnMainThread() override {
    HeadlessSelectFileDialogFactory::SetSelectFileDialogOnceCallbackForTests(
        base::BindOnce(
            &HeadlessModeInputSelectFileDialogTest::OnSelectFileDialogCallback,
            base::Unretained(this)));

    HeadlessModeProtocolBrowserTest::SetUpOnMainThread();
  }

  void FinishAsyncTest() override {
    EXPECT_TRUE(select_file_dialog_has_run_);

    HeadlessModeProtocolBrowserTest::FinishAsyncTest();
  }

 private:
  void OnSelectFileDialogCallback(ui::SelectFileDialog::Type type) {
    select_file_dialog_has_run_ = true;
  }

  bool select_file_dialog_has_run_ = false;
};

// TODO(crbug.com/40919351, crbug.com/443993825): flaky on Mac/Linux/Win.
HEADLESS_MODE_PROTOCOL_TEST_F(HeadlessModeInputSelectFileDialogTest,
                              DISABLED_InputSelectFileDialog,
                              "input/input-select-file-dialog.js")

class HeadlessModeScreencastTest : public HeadlessModeProtocolBrowserTest {
 public:
  HeadlessModeScreencastTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModeProtocolBrowserTest::SetUpCommandLine(command_line);

#if BUILDFLAG(IS_WIN)
    // Screencast tests fail on Windows unless GPU compositing is disabled,
    // see https://crbug.com/1411976 and https://crbug.com/1502651.
    UseSoftwareCompositing();
#endif
  }
};

HEADLESS_MODE_PROTOCOL_TEST_F(HeadlessModeScreencastTest,
                              ScreencastBasics,
                              "shared/screencast-basics.js")
HEADLESS_MODE_PROTOCOL_TEST_F(HeadlessModeScreencastTest,
                              ScreencastViewport,
                              "shared/screencast-viewport.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowWithNewContext,
                            "shared/window-with-new-context.js")

HEADLESS_MODE_PROTOCOL_TEST(HiddenTargetCreate,
                            "shared/hidden-target-create.js")
HEADLESS_MODE_PROTOCOL_TEST(HiddenTargetClose, "shared/hidden-target-close.js")
HEADLESS_MODE_PROTOCOL_TEST(HiddenTargetCreateInvalidParams,
                            "shared/hidden-target-create-invalid-params.js")
HEADLESS_MODE_PROTOCOL_TEST(HiddenTargetPageEnable,
                            "shared/hidden-target-page-enable.js")

HEADLESS_MODE_PROTOCOL_TEST(ChangeWindowSize, "shared/change-window-size.js")
HEADLESS_MODE_PROTOCOL_TEST(ChangeWindowState, "shared/change-window-state.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowOuterSize, "shared/window-outer-size.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowInnerSize, "shared/window-inner-size.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowInnerSizeScaled,
                            "shared/window-inner-size-scaled.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowInnerSizeLargerThanScreen,
                            "shared/window-inner-size-larger-than-screen.js")

HEADLESS_MODE_PROTOCOL_TEST(LargeBrowserWindowSize,
                            "shared/large-browser-window-size.js")

// These currently fail on Mac, see https://crbug.com/1488010
#if !BUILDFLAG(IS_MAC)
HEADLESS_MODE_PROTOCOL_TEST(MinimizeRestoreWindow,
                            "shared/minimize-restore-window.js")
HEADLESS_MODE_PROTOCOL_TEST(MaximizeRestoreWindow,
                            "shared/maximize-restore-window.js")
HEADLESS_MODE_PROTOCOL_TEST(FullscreenRestoreWindow,
                            "shared/fullscreen-restore-window.js")
#endif  // !BUILDFLAG(IS_MAC)

// This currently fails on Mac, see https://crbug.com/416088625
#if !BUILDFLAG(IS_MAC)
HEADLESS_MODE_PROTOCOL_TEST(MaximizedWindowSize,
                            "shared/maximized-window-size.js")
#endif  // !BUILDFLAG(IS_MAC)

// These currently fail on Mac, see https://crbug.com/1500046
#if !BUILDFLAG(IS_MAC)
HEADLESS_MODE_PROTOCOL_TEST(FullscreenWindowSize,
                            "shared/fullscreen-window-size.js")
HEADLESS_MODE_PROTOCOL_TEST(FullscreenWindowSizeScaled,
                            "shared/fullscreen-window-size-scaled.js")
#endif  // !BUILDFLAG(IS_MAC)

HEADLESS_MODE_PROTOCOL_TEST(PrintToPdfTinyPage,
                            "shared/print-to-pdf-tiny-page.js")

HEADLESS_MODE_PROTOCOL_TEST(ScreenDetailsMultipleScreens,
                            "shared/screen-details-multiple-screens.js")

HEADLESS_MODE_PROTOCOL_TEST(ScreenDetailsMultipleScreensScaled,
                            "shared/screen-details-multiple-screens-scaled.js")

HEADLESS_MODE_PROTOCOL_TEST(ScreenDetailsRotationAngle,
                            "shared/screen-details-rotation-angle.js")

HEADLESS_MODE_PROTOCOL_TEST(ScreenDetailsPixelRatio,
                            "shared/screen-details-pixel-ratio.js")

// TODO(crbug.com/442920826): Re-enable this test
#if BUILDFLAG(IS_WIN)
#define MAYBE_ScreenDetailsColorDepth DISABLED_ScreenDetailsColorDepth
#else
#define MAYBE_ScreenDetailsColorDepth ScreenDetailsColorDepth
#endif
HEADLESS_MODE_PROTOCOL_TEST(MAYBE_ScreenDetailsColorDepth,
                            "shared/screen-details-color-depth.js")

HEADLESS_MODE_PROTOCOL_TEST(ScreenDetailsWorkArea,
                            "shared/screen-details-work-area.js")

HEADLESS_MODE_PROTOCOL_TEST(ScreenDetailsWorkAreaScaled,
                            "shared/screen-details-work-area-scaled.js")

HEADLESS_MODE_PROTOCOL_TEST(RequestFullscreen, "shared/request-fullscreen.js")

// TODO(crbug.com/429035133): Times out on macOS. Fix and re-enable.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RequestFullscreenOnSecondaryScreen \
  DISABLED_RequestFullscreenOnSecondaryScreen
#else
#define MAYBE_RequestFullscreenOnSecondaryScreen \
  RequestFullscreenOnSecondaryScreen
#endif  // BUILDFLAG(IS_MAC)
HEADLESS_MODE_PROTOCOL_TEST(MAYBE_RequestFullscreenOnSecondaryScreen,
                            "shared/request-fullscreen-on-secondary-screen.js")

HEADLESS_MODE_PROTOCOL_TEST(CreateTargetPosition,
                            "shared/create-target-position.js")

HEADLESS_MODE_PROTOCOL_TEST(CreateTargetWindowState,
                            "shared/create-target-window-state.js")

HEADLESS_MODE_PROTOCOL_TEST(DocumentVisibilityState,
                            "shared/document-visibility-state.js")

HEADLESS_MODE_PROTOCOL_TEST(DocumentVisibilityStatePopup,
                            "shared/document-visibility-state-popup.js")

// Headless Mode uses Ozone only when running on Linux.
#if BUILDFLAG(IS_LINUX)
HEADLESS_MODE_PROTOCOL_TEST(OzoneScreenSizeOverride,
                            "sanity/ozone-screen-size-override.js")
#endif

// This currently results in an unexpected screen orientation type,
// see http://crbug.com/398150465.
HEADLESS_MODE_PROTOCOL_TEST(MultipleScreenDetails,
                            "shared/multiple-screen-details.js")

// TODO(crbug.com/40283476): MoveWindowBetweenScreens is failing on Mac
#if !BUILDFLAG(IS_MAC)
#define MAYBE_MoveWindowBetweenScreens MoveWindowBetweenScreens
#else
#define MAYBE_MoveWindowBetweenScreens DISABLED_MoveWindowBetweenScreens
#endif
HEADLESS_MODE_PROTOCOL_TEST(MAYBE_MoveWindowBetweenScreens,
                            "shared/move-window-between-screens.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowOpenOnSecondaryScreen,
                            "shared/window-open-on-secondary-screen.js")

// TODO(crbug.com/40283476): CreateTargetSecondaryScreen is failing on Mac
#if !BUILDFLAG(IS_MAC)
#define MAYBE_CreateTargetSecondaryScreen CreateTargetSecondaryScreen
#else
#define MAYBE_CreateTargetSecondaryScreen DISABLED_CreateTargetSecondaryScreen
#endif
HEADLESS_MODE_PROTOCOL_TEST(MAYBE_CreateTargetSecondaryScreen,
                            "shared/create-target-secondary-screen.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowOpenPopupPlacement,
                            "shared/window-open-popup-placement.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowSizeSwitchHandling,
                            "shared/window-size-switch-handling.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowSizeSwitchLargerThanScreen,
                            "shared/window-size-switch-larger-than-screen.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowScreenAvail, "shared/window-screen-avail.js")

// TODO(crbug.com/424797525): Fails Mac 13.
#if BUILDFLAG(IS_MAC)
#define MAYBE_StartFullscreenSwitch DISABLED_StartFullscreenSwitch
#else
#define MAYBE_StartFullscreenSwitch StartFullscreenSwitch
#endif

HEADLESS_MODE_PROTOCOL_TEST(MAYBE_StartFullscreenSwitch,
                            "sanity/start-fullscreen-switch.js")

// TODO(crbug.com/423951863): Fails on Mac 13.
#if BUILDFLAG(IS_MAC)
#define MAYBE_StartFullscreenSwitchScaled DISABLED_StartFullscreenSwitchScaled
#else
#define MAYBE_StartFullscreenSwitchScaled StartFullscreenSwitchScaled
#endif

HEADLESS_MODE_PROTOCOL_TEST(MAYBE_StartFullscreenSwitchScaled,
                            "sanity/start-fullscreen-switch-scaled.js")

// TODO(crbug.com/430156442): These fail on Mac 13
#if BUILDFLAG(IS_MAC)
#define MAYBE_WindowStateTransitions DISABLED_WindowStateTransitions
#define MAYBE_WindowZoomOnSecondaryScreen DISABLED_WindowZoomOnSecondaryScreen
#define MAYBE_WindowZoomSizeMatchesWorkArea \
  DISABLED_WindowZoomSizeMatchesWorkArea
#else
#define MAYBE_WindowStateTransitions WindowStateTransitions
#define MAYBE_WindowZoomOnSecondaryScreen WindowZoomOnSecondaryScreen
#define MAYBE_WindowZoomSizeMatchesWorkArea WindowZoomSizeMatchesWorkArea
#endif

HEADLESS_MODE_PROTOCOL_TEST(MAYBE_WindowStateTransitions,
                            "shared/window-state-transitions.js")

HEADLESS_MODE_PROTOCOL_TEST(MAYBE_WindowZoomOnSecondaryScreen,
                            "shared/window-zoom-on-secondary-screen.js")

HEADLESS_MODE_PROTOCOL_TEST(MAYBE_WindowZoomSizeMatchesWorkArea,
                            "shared/window-zoom-size-matches-work-area.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowScreenScaleFactor,
                            "shared/window-screen-scale-factor.js")

HEADLESS_MODE_PROTOCOL_TEST(WindowScreenSizeOrientation,
                            "shared/window-screen-size-orientation.js")

HEADLESS_MODE_PROTOCOL_TEST(AutofillTriggerCreditCard,
                            "autofill/autofill-trigger-credit-card.js")

HEADLESS_MODE_PROTOCOL_TEST(DispatchMouseEventScreenCoordinates,
                            "shared/dispatch-mouse-event-screen-coordinates.js")

HEADLESS_MODE_PROTOCOL_TEST(DispatchTouchEventScreenCoordinates,
                            "shared/dispatch-touch-event-screen-coordinates.js")

HEADLESS_MODE_PROTOCOL_TEST(
    EmulateTouchFromMouseEventScreenCoordinates,
    "shared/emulate-touch-from-mouse-event-screen-coordinates.js")

HEADLESS_MODE_PROTOCOL_TEST(GetScreenInfos, "shared/get-screen-infos.js")

HEADLESS_MODE_PROTOCOL_TEST(AddScreen, "shared/add-screen.js")

HEADLESS_MODE_PROTOCOL_TEST(AddScreenScaleFactor,
                            "shared/add-screen-scale-factor.js")

HEADLESS_MODE_PROTOCOL_TEST(AddScreenWorkArea, "shared/add-screen-work-area.js")

HEADLESS_MODE_PROTOCOL_TEST(AddScreenGetScreenDetails,
                            "shared/add-screen-get-screen-details.js")

HEADLESS_MODE_PROTOCOL_TEST(RemoveScreen, "shared/remove-screen.js")

HEADLESS_MODE_PROTOCOL_TEST(RemoveScreenGetScreenDetails,
                            "shared/remove-screen-get-screen-details.js")

HEADLESS_MODE_PROTOCOL_TEST(AddRemoveScreen, "shared/add-remove-screen.js")

// TODO(crbug.com/423951863): Fails on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SetZoomedWindowBounds DISABLED_SetZoomedWindowBounds
#else
#define MAYBE_SetZoomedWindowBounds SetZoomedWindowBounds
#endif
HEADLESS_MODE_PROTOCOL_TEST(MAYBE_SetZoomedWindowBounds,
                            "shared/set-zoomed-window-bounds.js")
}  // namespace headless

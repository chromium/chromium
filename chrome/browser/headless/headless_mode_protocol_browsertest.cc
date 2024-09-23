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
#include "build/build_config.h"
#include "chrome/browser/headless/test/headless_browser_test_utils.h"
#include "components/headless/select_file_dialog/headless_select_file_dialog.h"
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
static const char kDumpConsoleMessages[] = "dump-console-messages";
static const char kDumpDevToolsProtocol[] = "dump-devtools-protocol";
static const char kDumpTestResult[] = "dump-test-result";
}  // namespace switches

namespace {
static const base::FilePath kTestsScriptRoot(
    FILE_PATH_LITERAL("chrome/browser/headless/test/data/protocol"));
}  // namespace

HeadlessModeProtocolBrowserTest::HeadlessModeProtocolBrowserTest() = default;
HeadlessModeProtocolBrowserTest::~HeadlessModeProtocolBrowserTest() = default;

void HeadlessModeProtocolBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(::network::switches::kHostResolverRules,
                                  "MAP *.test 127.0.0.1");
  HeadlessModeDevTooledBrowserTest::SetUpCommandLine(command_line);
}

base::Value::Dict HeadlessModeProtocolBrowserTest::GetPageUrlExtraParams() {
  return base::Value::Dict();
}

void HeadlessModeProtocolBrowserTest::RunTestScript(
    std::string_view script_name) {
  test_folder_ = "/protocol/";
  script_name_ = script_name;
  RunTest();
}

void HeadlessModeProtocolBrowserTest::RunDevTooledTest() {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "third_party/blink/web_tests/http/tests/inspector-protocol");
  ASSERT_TRUE(embedded_test_server()->Start());

  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(web_contents_.get());

  // Set up Page domain.
  devtools_client_.AddEventHandler(
      "Page.loadEventFired",
      base::BindRepeating(&HeadlessModeProtocolBrowserTest::OnLoadEventFired,
                          base::Unretained(this)));
  devtools_client_.SendCommand("Page.enable");

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDumpConsoleMessages)) {
    // Set up Runtime domain to intercept console messages.
    devtools_client_.AddEventHandler(
        "Runtime.consoleAPICalled",
        base::BindRepeating(
            &HeadlessModeProtocolBrowserTest::OnConsoleAPICalled,
            base::Unretained(this)));
    devtools_client_.SendCommand("Runtime.enable");
  }

  // Expose DevTools protocol to the target.
  browser_devtools_client_.SendCommand("Target.exposeDevToolsProtocol",
                                       Param("targetId", agent_host->GetId()));

  // Navigate to test harness page
  GURL page_url = embedded_test_server()->GetURL(
      "harness.test", "/protocol/inspector-protocol-test.html");
  devtools_client_.SendCommand("Page.navigate", Param("url", page_url.spec()));
}

void HeadlessModeProtocolBrowserTest::OnLoadEventFired(
    const base::Value::Dict& params) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath src_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
  base::FilePath test_path =
      src_dir.Append(kTestsScriptRoot).AppendASCII(script_name_);
  std::string script;
  if (!base::ReadFileToString(test_path, &script)) {
    ADD_FAILURE() << "Unable to read test in " << test_path;
    FinishAsyncTest();
    return;
  }
  GURL test_url = embedded_test_server()->GetURL("harness.test",
                                                 "/protocol/" + script_name_);
  GURL target_url =
      embedded_test_server()->GetURL("127.0.0.1", "/protocol/" + script_name_);

  base::Value::Dict test_params;
  test_params.Set("test", test_url.spec());
  test_params.Set("target", target_url.spec());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDumpDevToolsProtocol)) {
    test_params.Set("dumpDevToolsProtocol", true);
  }
  test_params.Merge(GetPageUrlExtraParams());

  std::string json_test_params;
  base::JSONWriter::Write(test_params, &json_test_params);
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
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDumpTestResult)) {
    LOG(INFO) << "Test result: " << params.DebugString();
  }

  std::string* value = params.FindStringByDottedPath("result.result.value");
  EXPECT_THAT(value, NotNull());

  ProcessTestResult(*value);

  FinishAsyncTest();
}

// TODO(crbug.com/40253719): Move similar code in //headless/test to a shared
// location in //components/devtools/test.
void HeadlessModeProtocolBrowserTest::ProcessTestResult(
    const std::string& test_result) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
  base::FilePath expectation_path =
      src_dir.Append(kTestsScriptRoot)
          .AppendASCII(script_name_.substr(0, script_name_.length() - 3) +
                       "-expected.txt");

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

void HeadlessModeProtocolBrowserTest::OnConsoleAPICalled(
    const base::Value::Dict& params) {
  const base::Value::List* args = params.FindListByDottedPath("params.args");
  if (!args || args->empty()) {
    return;
  }

  const base::Value* value = args->front().GetDict().Find("value");
  switch (value->type()) {
    case base::Value::Type::NONE:
    case base::Value::Type::BOOLEAN:
    case base::Value::Type::INTEGER:
    case base::Value::Type::DOUBLE:
    case base::Value::Type::STRING:
      LOG(INFO) << value->DebugString();
      return;
    default:
      LOG(INFO) << "Unhandled value type: " << value->type();
      return;
  }
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

// TODO(crbug.com/40919351): flaky on Mac and Linux builders.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_InputSelectFileDialog DISABLED_InputSelectFileDialog
#else
#define MAYBE_InputSelectFileDialog InputSelectFileDialog
#endif
HEADLESS_MODE_PROTOCOL_TEST_F(HeadlessModeInputSelectFileDialogTest,
                              MAYBE_InputSelectFileDialog,
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
                              "sanity/screencast-basics.js")
HEADLESS_MODE_PROTOCOL_TEST_F(HeadlessModeScreencastTest,
                              ScreencastViewport,
                              "sanity/screencast-viewport.js")

HEADLESS_MODE_PROTOCOL_TEST(LargeBrowserWindowSize,
                            "sanity/large-browser-window-size.js")

// These currently fail on Mac,see https://crbug.com/1488010
#if !BUILDFLAG(IS_MAC)
HEADLESS_MODE_PROTOCOL_TEST(MinimizeRestoreWindow,
                            "sanity/minimize-restore-window.js")
HEADLESS_MODE_PROTOCOL_TEST(MaximizeRestoreWindow,
                            "sanity/maximize-restore-window.js")
HEADLESS_MODE_PROTOCOL_TEST(FullscreenRestoreWindow,
                            "sanity/fullscreen-restore-window.js")
#endif  // !BUILDFLAG(IS_MAC)

HEADLESS_MODE_PROTOCOL_TEST(MaximizedWindowSize,
                            "sanity/maximized-window-size.js")

// This currently fails on Mac,see https://crbug.com/1500046
#if !BUILDFLAG(IS_MAC)
HEADLESS_MODE_PROTOCOL_TEST(FullscreenWindowSize,
                            "sanity/fullscreen-window-size.js")
#endif  // !BUILDFLAG(IS_MAC)

HEADLESS_MODE_PROTOCOL_TEST(PrintToPdfTinyPage,
                            "sanity/print-to-pdf-tiny-page.js")

HEADLESS_MODE_PROTOCOL_TEST(RequestFullscreen, "sanity/request-fullscreen.js")
}  // namespace headless

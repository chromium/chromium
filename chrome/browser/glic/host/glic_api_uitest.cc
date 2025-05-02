// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include <algorithm>
#include <deque>
#include <ranges>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/metrics/metrics_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file runs the respective JS tests from
// chrome/test/data/webui/glic/api_test.ts.

#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)
#define SLOW_BINARY
#endif

namespace glic {
namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTab);
std::vector<std::string> GetTestSuiteNames() {
  return {
      "GlicApiTest",
      "GlicApiTestWithOneTab",
      "GlicApiTestWithFastTimeout",
  };
}

// Observes the state of the WebUI hosted in the glic window.
class WebUIStateListener : public GlicWindowController::WebUiStateObserver {
 public:
  explicit WebUIStateListener(GlicWindowController* controller)
      : controller_(controller) {
    controller_->AddWebUiStateObserver(this);
    states_.push_back(controller_->GetWebUiState());
  }

  ~WebUIStateListener() override {
    controller_->RemoveWebUiStateObserver(this);
  }

  void WebUiStateChanged(mojom::WebUiState state) override {
    states_.push_back(state);
  }

  // Returns if `state` has been seen. Consumes all observed states up to the
  // point where this state is seen.
  void WaitForWebUiState(mojom::WebUiState state) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      while (!states_.empty()) {
        if (states_.front() != state) {
          states_.pop_front();
          continue;
        }
        return true;
      }
      return false;
    })) << "Timed out waiting for WebUI state "
        << state << ". State =" << controller_->GetWebUiState();
  }

 private:
  raw_ptr<GlicWindowController> controller_;
  std::deque<mojom::WebUiState> states_;
};

struct ExecuteTestOptions {
  // Test parameters passed to the JS test. See `ApiTestFixtureBase.testParams`.
  base::Value params;

  // Assert that the test function does not return, and instead destroys the
  // test frame.
  bool expect_guest_frame_destroyed = false;

  // Whether to wait for the guest before starting the test.
  // TODO(harringtond): This should always be true, but I'm seeing this error
  // for tests where wait_for_guest needs to be overridden:
  //   DCHECK failed: false. Non-Profile BrowserContext passed to
  //   Profile::FromBrowserContext! If you have a test linked in chrome/ you
  //   need a chrome/ based test class such as TestingProfile in
  //   chrome/test/base/testing_profile.h or you need to subclass your test
  //   class from Profile, not from BrowserContext.
  bool wait_for_guest = true;
};

class GlicApiTest : public test::InteractiveGlicTest {
 public:
  GlicApiTest() {
    add_mock_glic_query_param(
        "test",
        ::testing::UnitTest::GetInstance()->current_test_info()->name());

    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlic,
          {
              {"glic-default-hotkey", "Ctrl+G"},
              // Shorten load timeouts.
              {features::kGlicPreLoadingTimeMs.name, "20"},
              {features::kGlicMinLoadingTimeMs.name, "40"},
          }},
         {features::kGlicScrollTo, {}},
         {features::kGlicUserResize, {}}},
        /*disabled_features=*/
        {
            features::kGlicWarming,
        });

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kGlicHostLogging);
    SetGlicPagePath("/glic/test.html");
  }
  ~GlicApiTest() override = default;

  void TearDownOnMainThread() override {
    if (next_step_required_) {
      FAIL() << "Test not finished: call ContinueJsTest()";
    }
    test::InteractiveGlicTest::TearDownOnMainThread();
  }

  // Run the test typescript function. The typescript function must have the
  // same name as the current test.
  // If the test uses `advanceToNextStep()`, then ContinueJsTest() must be
  // called later.
  void ExecuteJsTest(ExecuteTestOptions options = {}) {
    if (options.wait_for_guest) {
      WaitForGuest();
    }
    content::RenderFrameHost* glic_guest_frame = FindGlicGuestMainFrame();
    ASSERT_TRUE(glic_guest_frame);
    std::string param_json;
    base::JSONWriter::Write(options.params, &param_json);
    ProcessTestResult(
        options,
        content::EvalJs(glic_guest_frame,
                        base::StrCat({"runApiTest(", param_json, ")"})));
  }

  // Continues test execution if `advanceToNextStep()` was used to return
  // control to C++.
  void ContinueJsTest(ExecuteTestOptions options = {}) {
    ASSERT_TRUE(next_step_required_);
    content::RenderFrameHost* glic_guest_frame = FindGlicGuestMainFrame();
    next_step_required_ = false;
    ASSERT_TRUE(glic_guest_frame);
    std::string param_json;
    base::JSONWriter::Write(options.params, &param_json);
    ProcessTestResult(
        options,
        content::EvalJs(glic_guest_frame,
                        base::StrCat({"continueApiTest(", param_json, ")"})));
  }

  void WaitForGuest() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return FindGlicGuestMainFrame() != nullptr;
    })) << "Timed out waiting for the frame";
    auto end_time = base::TimeTicks::Now() + base::Seconds(5);
    while (base::TimeTicks::Now() < end_time) {
      content::RenderFrameHost* frame = FindGlicGuestMainFrame();
      ASSERT_TRUE(frame) << "Guest frame deleted";
      auto result =
          content::EvalJs(frame, {"typeof runApiTest !== 'undefined'"});
      if (result.error.empty() && result.ExtractBool()) {
        return;
      }
      base::RunLoop run_loop;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(200));
      run_loop.Run();
    }
    FAIL() << "Timed out waiting for guest frame";
  }

  content::RenderFrameHost* FindGlicGuestMainFrame() {
    GlicKeyedService* glic =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
    for (GlicPageHandler* handler : glic->GetPageHandlersForTesting()) {
      if (handler->GetGuestMainFrame()) {
        return handler->GetGuestMainFrame();
      }
    }
    return nullptr;
  }

  void WaitForWebUiState(mojom::WebUiState state) {
    WebUIStateListener listener(&window_controller());
    listener.WaitForWebUiState(state);
  }

  const std::optional<base::Value>& step_data() const { return step_data_; }

 private:
  void ProcessTestResult(const ExecuteTestOptions& options,
                         const content::EvalJsResult& result) {
    if (options.expect_guest_frame_destroyed) {
      ASSERT_THAT(result.error, testing::HasSubstr("RenderFrame deleted."));
      return;
    }

    ASSERT_THAT(result, content::EvalJsResult::IsOk());
    if (result.value.is_dict()) {
      auto* id = result.value.GetDict().Find("id");
      if (id && id->is_string() && id->GetString() == "next-step") {
        step_data_ = result.value.GetDict().Find("payload")->Clone();
      }
      next_step_required_ = true;
      return;
    }
    ASSERT_THAT(result.ExtractString(), testing::Eq("pass"));
  }

  bool next_step_required_ = false;
  std::optional<base::Value> step_data_;
  base::test::ScopedFeatureList features_;
};

class GlicApiTestWithOneTab : public GlicApiTest {
 public:
  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();

    // Load the test page in a tab, so that there is some page context.
    GURL page_url =
        InProcessBrowserTest::embedded_test_server()->GetURL("/glic/test.html");
    RunTestSequence(InstrumentTab(kFirstTab),
                    NavigateWebContents(kFirstTab, page_url),
                    OpenGlicWindow(GlicWindowMode::kDetached,
                                   GlicInstrumentMode::kHostAndContents));
  }
};

class GlicApiTestWithFastTimeout : public GlicApiTest {
 public:
  GlicApiTestWithFastTimeout() {
    features2_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{
            features::kGlic,
            {
// For slow binaries, use a longer timeout.
#if defined(SLOW_BINARY)
                {features::kGlicMaxLoadingTimeMs.name, "6000"},
#else
                {features::kGlicMaxLoadingTimeMs.name, "3000"},
#endif
            },
        }},
        /*disabled_features=*/
        {});
  }

 private:
  base::test::ScopedFeatureList features2_;
};

// Note: Test names must match test function names in api_test.ts.

// TODO(harringtond): Many of these tests are minimal, and could be improved
// with additional cases and additional assertions.

// Just verify the test harness works.
IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testDoNothing) {
  ExecuteJsTest();
}

// Checks that all tests in api_test.ts have a corresponding test case in this
// file.
IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testAllTestsAreRegistered) {
  ExecuteJsTest();
  ASSERT_TRUE(step_data()->is_list());
  ::testing::UnitTest* unit_test = ::testing::UnitTest::GetInstance();
  std::set<std::string> test_suites;
  const std::vector<std::string> suite_names = GetTestSuiteNames();
  std::set<std::string> js_test_names, cc_test_names;
  for (const auto& test_name : step_data()->GetList()) {
    js_test_names.insert(test_name.GetString());
  }
  for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
    const auto* test_suite = unit_test->GetTestSuite(i);
    if (!base::Contains(suite_names, std::string(test_suite->name()))) {
      continue;
    }
    for (int j = 0; j < test_suite->total_test_count(); ++j) {
      std::string name = test_suite->GetTestInfo(j)->name();
      if (name.starts_with("DISABLED_")) {
        cc_test_names.insert(name.substr(9));
      } else {
        cc_test_names.insert(name);
      }
    }
  }
  ASSERT_THAT(js_test_names, testing::IsSubsetOf(cc_test_names))
      << "Test cases in js, but not cc";
  ContinueJsTest();
}

// Test fails in tear-down. Leaving disabled for branch.
IN_PROC_BROWSER_TEST_F(GlicApiTest, DISABLED_testLoadWhileWindowClosed) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  window_controller().Close();
  ExecuteJsTest();
  // Make sure the WebUI transitions to kReady, otherwise the web client may be
  // destroyed.
  WaitForWebUiState(mojom::WebUiState::kReady);
}

// TODO(harringtond): This is a flaky.
IN_PROC_BROWSER_TEST_F(GlicApiTest, DISABLED_testInitializeFailsWindowClosed) {
  // Immediately close the window to check behavior while window is closed.
  // Fail client initialization, should see error page.
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  window_controller().Close();
  ExecuteJsTest();
  WaitForWebUiState(mojom::WebUiState::kError);
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testInitializeFailsWindowOpen) {
  // Fail client initialization, should see error page.
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "error")),
  });
  WaitForWebUiState(mojom::WebUiState::kError);

  // Closing and reopening the window should trigger a retry. This time the
  // client initializes correctly.
  window_controller().Close();
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "none")),
  });
  WaitForWebUiState(mojom::WebUiState::kReady);
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testReload) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(&window_controller());
  ExecuteJsTest({
      .params = base::Value(
          base::Value::Dict().Set("failWith", "reloadAfterInitialize")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "none")),
  });
}

// TODO(harringtond): This is a flaky.
IN_PROC_BROWSER_TEST_F(GlicApiTest, DISABLED_testInitializeFailsAfterReload) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(&window_controller());
  ExecuteJsTest({
      .params = base::Value(
          base::Value::Dict().Set("failWith", "reloadAfterInitialize")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "error")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kError);
}

// TODO(harringtond): This is a flaky.
IN_PROC_BROWSER_TEST_F(GlicApiTestWithFastTimeout,
                       DISABLED_testInitializeTimesOut) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(&window_controller());
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "timeout")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kError);
#endif
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testCreateTab) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckTabCount(1));
  ExecuteJsTest();
  RunTestSequence(CheckTabCount(2));
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testOpenGlicSettingsPage) {
  ExecuteJsTest();

  RunTestSequence(
      InstrumentTab(kSettingsTab),
      WaitForWebContentsReady(
          kSettingsTab, chrome::GetSettingsUrl(chrome::kGlicSettingsSubpage)));
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testClosePanel) {
  ExecuteJsTest();
  RunTestSequence(WaitForHide(kGlicViewElementId));
}

// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true.
IN_PROC_BROWSER_TEST_F(GlicApiTest, DISABLED_testAttachPanel) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
  RunTestSequence(CheckControllerWidgetMode(GlicWindowMode::kAttached));
}

// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true.
IN_PROC_BROWSER_TEST_F(GlicApiTest, DISABLED_testUnsubscribeFromObservable) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
}

// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true.
IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, DISABLED_testDetachPanel) {
  ExecuteJsTest();
  RunTestSequence(CheckControllerWidgetMode(GlicWindowMode::kDetached));
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testShowProfilePicker) {
  base::test::TestFuture<void> profile_picker_opened;
  ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
      profile_picker_opened.GetCallback());
  ExecuteJsTest();
  ASSERT_TRUE(profile_picker_opened.Wait());
  // TODO(harringtond): Try to test changing profiles.
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testPanelActive) {
  ExecuteJsTest();

  // Opening a new browser window will deactivate the previous one, and make
  // the panel not active.
  NavigateParams params(browser()->profile(), GURL("about:blank"),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      Navigate(&params);

  ContinueJsTest();
}

// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true.
IN_PROC_BROWSER_TEST_F(GlicApiTest, DISABLED_testCanAttachPanel) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
  // TODO(harringtond): Test case where the canAttachPanel returns false.
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testIsBrowserOpen) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));

  ExecuteJsTest();

  // Open a new incognito tab so that Chrome doesn't exit, and close the first
  // browser.
  CreateIncognitoBrowser();
  CloseBrowserAsynchronously(browser());

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testEnableDragResize) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
  RunTestSequence(InAnyContext(ExpectUserCanResize(true)));
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testDisableDragResize) {
  // Check the default resize setting here.
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  ExpectUserCanResize(true));
  ExecuteJsTest();
  RunTestSequence(InAnyContext(ExpectUserCanResize(false)));
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testInitiallyNotResizable) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
  RunTestSequence(InAnyContext(ExpectUserCanResize(false)));
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testGetFocusedTabState) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testGetFocusedTabStateV2) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testGetFocusedTabStateV2BrowserClosed) {
  // Note: ideally this test would only open Glic after the main browser is
  // closed. This however crashes in `OpenGlicWindow()`.
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));

  // Open a new incognito window first so that Chrome doesn't exit, then close
  // the first browser window.
  CreateIncognitoBrowser();
  CloseBrowserAsynchronously(browser());

  ExecuteJsTest({.wait_for_guest = false});
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithoutPermission) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithNoRequestedData) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithAllRequestedData) {
  ExecuteJsTest();
}

// TODO(harringtond): Fix this, it hangs.
IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, DISABLED_testCaptureScreenshot) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testPermissionAccess) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testGetUserProfileInfo) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testRefreshSignInCookies) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testSetContextAccessIndicator) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testSetAudioDucking) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testMetrics) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testScrollToFindsText) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testScrollToNoMatchFound) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testSetSyntheticExperimentState) {
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([]() {
    std::vector<variations::ActiveGroupId> trials =
        g_browser_process->metrics_service()
            ->GetSyntheticTrialRegistry()
            ->GetCurrentSyntheticFieldTrialsForTest();
    variations::ActiveGroupId expected =
        variations::MakeActiveGroupId("TestTrial", "Enabled");
    return std::ranges::any_of(trials, [&](const auto& trial) {
      return trial.name == expected.name && trial.group == expected.group;
    });
  }));
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testSetSyntheticExperimentStateMultiProfile) {
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([]() {
    std::vector<variations::ActiveGroupId> trials =
        g_browser_process->metrics_service()
            ->GetSyntheticTrialRegistry()
            ->GetCurrentSyntheticFieldTrialsForTest();
    variations::ActiveGroupId expected =
        variations::MakeActiveGroupId("TestTrial", "MultiProfileDetected");
    return std::ranges::any_of(trials, [&](const auto& trial) {
      return trial.name == expected.name && trial.group == expected.group;
    });
  }));

  // Now cut log file and see if Group2 is enabled.
  g_browser_process->metrics_service()->NotifyLogsEventManagerForTesting(
      metrics::MetricsLogsEventManager::LogEvent::kLogCreated, "Fakehash",
      "Fake log created message...");

  // Check that last registered group is registered on new log file...
  ASSERT_TRUE(base::test::RunUntil([]() {
    std::vector<variations::ActiveGroupId> trials =
        g_browser_process->metrics_service()
            ->GetSyntheticTrialRegistry()
            ->GetCurrentSyntheticFieldTrialsForTest();
    variations::ActiveGroupId expected =
        variations::MakeActiveGroupId("TestTrial", "Group2");
    return std::ranges::any_of(trials, [&](const auto& trial) {
      return trial.name == expected.name && trial.group == expected.group;
    });
  }));
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testNotifyPanelWillOpenIsCalledOnce) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testGetOsHotkeyState) {
  ExecuteJsTest();
  g_browser_process->local_state()->SetString(prefs::kGlicLauncherHotkey,
                                              "Ctrl+Shift+1");
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testSetWindowDraggableAreas) {
  ExecuteJsTest();
  const int x = 10;
  const int y = 20;
  const int width = 30;
  const int height = 40;

  RunTestSequence(
      // Test points within the draggable area.
      CheckPointIsWithinDraggableArea(gfx::Point(x, y), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width - 1, y), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y + height - 1), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width - 1, y + height - 1),
                                      true),
      // Test points at the edges of the draggable area.
      CheckPointIsWithinDraggableArea(gfx::Point(x - 1, y), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y - 1), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width, y), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y + height), false));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testSetWindowDraggableAreasDefault) {
  // TODO(crbug.com/404845792): Default draggable area is currently hardcoded in
  // glic_page_handler.cc. This should be moved to a shared location and updated
  // here.
  const int x = 0;
  const int y = 0;
  const int width = 400;
  const int height = 80;

  ExecuteJsTest();
  RunTestSequence(
      // Test points within the draggable area.
      CheckPointIsWithinDraggableArea(gfx::Point(x, y), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width - 1, y), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y + height - 1), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width - 1, y + height - 1),
                                      true),
      // Test points at the edges of the draggable area.
      CheckPointIsWithinDraggableArea(gfx::Point(x - 1, y), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y - 1), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width, y), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y + height), false));
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testSetMinimumWidgetSize) {
  ExecuteJsTest();
  ASSERT_TRUE(step_data()->is_dict());
  const auto& min_size = step_data()->GetDict();
  const int width = min_size.FindInt("width").value();
  const int height = min_size.FindInt("height").value();

  RunTestSequence(CheckWidgetMinimumSize(gfx::Size(width, height)));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testNavigateToDifferentClientPage) {
  WebUIStateListener listener(&window_controller());
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(0)});  // test run count: 0.
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(1)});  // test run count: 1.
}

// TODO(crbug.com/410881522): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_testNavigateToBadPage DISABLED_testNavigateToBadPage
#else
#define MAYBE_testNavigateToBadPage testNavigateToBadPage
#endif
IN_PROC_BROWSER_TEST_F(GlicApiTestWithFastTimeout,
                       MAYBE_testNavigateToBadPage) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  // Client loads, and navigates to a new URL. We try to load the client again,
  // but it fails.
  WebUIStateListener listener(&window_controller());
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(0)});
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  listener.WaitForWebUiState(mojom::WebUiState::kError);

  // Open the glic window to trigger reloading the client.
  // This time the client should load, falling back to the original URL.
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest({.params = base::Value(1)});
#endif
}

}  // namespace
}  // namespace glic

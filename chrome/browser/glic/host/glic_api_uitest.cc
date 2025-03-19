// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include <algorithm>
#include <ranges>

#include "base/command_line.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/glic/interactive_glic_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/metrics/metrics_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
std::vector<std::string> GetTestSuiteNames() {
  return {
      "GlicApiTest",
      "GlicApiTestWithOneTab",
  };
}

class GlicApiTest : public test::InteractiveGlicTest {
 public:
  GlicApiTest() {
    add_mock_glic_query_param(
        "test",
        ::testing::UnitTest::GetInstance()->current_test_info()->name());

    features_.InitWithFeatures(
        {features::kGlicScrollTo, features::kGlicUserResize}, {});
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
  void ExecuteJsTest() {
    content::WebContents* web_contents = FindGlicGuestWebContents();
    ASSERT_TRUE(web_contents);
    auto result = content::EvalJs(web_contents, base::StrCat({"runApiTest()"}));
    ASSERT_THAT(result, content::EvalJsResult::IsOk());
    ProcessTestResult(result);
  }

  // Continues test execution if `advanceToNextStep()` was used to return
  // control to C++.
  void ContinueJsTest() {
    ASSERT_TRUE(next_step_required_);
    content::WebContents* web_contents = FindGlicGuestWebContents();
    next_step_required_ = false;
    ASSERT_TRUE(web_contents);
    auto result =
        content::EvalJs(web_contents, base::StrCat({"continueApiTest()"}));
    ProcessTestResult(result);
  }

  content::WebContents* FindGlicGuestWebContents() {
    GlicKeyedService* glic =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
    for (GlicPageHandler* handler : glic->GetPageHandlersForTesting()) {
      if (handler->guest_contents()) {
        return handler->guest_contents();
      }
    }
    return nullptr;
  }

  const std::optional<base::Value>& step_data() const { return step_data_; }

 private:
  void ProcessTestResult(const content::EvalJsResult& result) {
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
                    OpenGlicWindow(GlicWindowMode::kAttached,
                                   GlicInstrumentMode::kHostAndContents));
  }
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

IN_PROC_BROWSER_TEST_F(GlicApiTest, testCreateTab) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
  // TODO(harringtond): Add assertions to verify a tab was created.
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testOpenGlicSettingsPage) {
  ExecuteJsTest();
  // TODO(harringtond): Add assertions to verify the settings page opened.
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testClosePanel) {
  ExecuteJsTest();
  RunTestSequence(WaitForHide(kGlicViewElementId));
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testAttachPanel) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
  RunTestSequence(CheckControllerWidgetMode(GlicWindowMode::kAttached));
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testUnsubscribeFromObservable) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testDetachPanel) {
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

IN_PROC_BROWSER_TEST_F(GlicApiTest, testCanAttachPanel) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
  // TODO(harringtond): Test case where the canAttachPanel returns false.
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testEnableDragResize) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
  RunTestSequence(InAnyContext(ExpectUserCanResize(true)));
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testDisableDragResize) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
  RunTestSequence(InAnyContext(ExpectUserCanResize(false)));
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testGetFocusedTabStateV2) {
  ExecuteJsTest();
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
                       testNotifyPanelWillOpenIsCalledOnce) {
  ExecuteJsTest();
}

}  // namespace
}  // namespace glic

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include <algorithm>
#include <deque>
#include <ranges>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/mock_contextual_cueing_service.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/media/audio_ducker.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/permissions/system/mock_platform_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/metrics/metrics_service.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/glic_page_context_eligibility_metadata.pb.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"

// This file runs the respective JS tests from
// chrome/test/data/webui/glic/api_test.ts.

#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
#define SLOW_BINARY
#endif

namespace glic {
namespace {
using testing::_;
using testing::Contains;
using testing::Pair;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTab);
std::vector<std::string> GetTestSuiteNames() {
  return {
      "GlicApiTest",
      "GlicApiTestWithOneTab",
      "GlicApiTestWithFastTimeout",
      "GlicApiTestSystemSettingsTest",
      "GlicApiTestWithOneTabAndContextualCueing",
      "GlicApiTestWithOneTabAndPreloading",
      "GlicApiTestPageContextEligibilityTest",
  };
}

// Observes the state of the WebUI hosted in the glic window.
class WebUIStateListener : public Host::Observer {
 public:
  explicit WebUIStateListener(Host* host) : host_(host) {
    host_->AddObserver(this);
    states_.push_back(host_->GetPrimaryWebUiState());
  }

  ~WebUIStateListener() override { host_->RemoveObserver(this); }

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
        << state << ". State =" << host_->GetPrimaryWebUiState();
  }

 private:
  raw_ptr<Host> host_;
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

class GlicApiTest : public NonInteractiveGlicTest {
 public:
  GlicApiTest() {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &GlicApiTest::SorryPageRequestHandler, base::Unretained(this)));

    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &GlicApiTest::OnEmbeddedTestServerHttpRequest, base::Unretained(this)));

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
         {features::kGlicClosedCaptioning, {}},
         {features::kGlicApiActivationGating, {}}},
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
        content::EvalJs(
            glic_guest_frame,
            base::StrCat(
                {"runApiTest(",
                 base::NumberToString((TestTimeouts::action_max_timeout() * 0.9)
                                          .InMilliseconds()),
                 ",", param_json, ")"})));
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
    auto end_time = base::TimeTicks::Now() + base::Seconds(10);
    content::RenderFrameHost* frame = nullptr;
    while (base::TimeTicks::Now() < end_time) {
      // Note: Sometimes the previous guest frame is still around, but it won't
      // have the runApiTest function. Loop until both conditions are met.
      frame = FindGlicGuestMainFrame();
      if (frame) {
        auto result =
            content::EvalJs(frame, {"typeof runApiTest !== 'undefined'"});
        if (result.error.empty() && result.ExtractBool()) {
          return;
        }
      }
      base::RunLoop run_loop;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(200));
      run_loop.Run();
    }
    FAIL() << "Timed out waiting for guest frame. Guest frame: "
           << (frame ? frame->GetLastCommittedURL().spec() : "not found");
  }

  void WaitForWebUiState(mojom::WebUiState state) {
    WebUIStateListener listener(&host());
    listener.WaitForWebUiState(state);
  }

  const std::optional<base::Value>& step_data() const { return step_data_; }

 protected:
  // Just an error page at a specific /sorry/... URL.
  std::unique_ptr<net::test_server::HttpResponse> SorryPageRequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.method != net::test_server::METHOD_GET ||
        !base::StartsWith(request.relative_url, "/sorry/index.html")) {
      return nullptr;
    }
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_code(net::HttpStatusCode::HTTP_OK);
    result->set_content_type("text/html");
    result->set_content("Sorry!");
    return result;
  }

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

  // Records all requests to the embedded test server.
  void OnEmbeddedTestServerHttpRequest(
      const net::test_server::HttpRequest& request) {
    embedded_test_server_requests_.push_back(request);
  }

  std::vector<net::test_server::HttpRequest> embedded_test_server_requests_;
  bool next_step_required_ = false;
  std::optional<base::Value> step_data_;
  base::test::ScopedFeatureList features_;
};

class GlicApiTestWithOneTab : public GlicApiTest {
 public:
  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();

    // Load the test page in a tab, so that there is some page context.
    RunTestSequence(InstrumentTab(kFirstTab),
                    NavigateWebContents(kFirstTab, page_url()),
                    OpenGlicWindow(GlicWindowMode::kDetached,
                                   GlicInstrumentMode::kHostAndContents));
  }

  GURL page_url() {
    return InProcessBrowserTest::embedded_test_server()->GetURL(
        "/glic/test.html");
  }
};

class GlicApiTestWithOneTabAndPreloading : public GlicApiTestWithOneTab {
 public:
  GlicApiTestWithOneTabAndPreloading() {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlic,
          {
              {"glic-default-hotkey", "Ctrl+G"},
              // Shorten load timeouts.
              {features::kGlicPreLoadingTimeMs.name, "20"},
              {features::kGlicMinLoadingTimeMs.name, "40"},
          }},
         {features::kGlicApiActivationGating, {}},
         {features::kGlicWarming,
          {{features::kGlicWarmingDelayMs.name, "0"},
           {features::kGlicWarmingJitterMs.name, "0"}}}},
        /*disabled_features=*/
        {});
    // This will temporarily disable preloading to ensure that we don't load the
    // web client before we've initialized the embedded test server and can set
    // the correct URL.
    GlicProfileManager::ForceMemoryPressureForTesting(
        base::MemoryPressureMonitor::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_CRITICAL);
    GlicProfileManager::ForceConnectionTypeForTesting(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);
  }

  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();
    RunTestSequence(InstrumentTab(kFirstTab),
                    NavigateWebContents(kFirstTab, page_url()));
  }

  void TearDown() override {
    GlicApiTestWithOneTab::TearDown();
    GlicProfileManager::ForceMemoryPressureForTesting(std::nullopt);
    GlicProfileManager::ForceConnectionTypeForTesting(std::nullopt);
  }

  GlicKeyedService* GetService() {
    Profile* profile = browser()->profile();
    return GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  }

  Host* GetHost() {
    Profile* profile = browser()->profile();
    return &GlicKeyedServiceFactory::GetGlicKeyedService(profile)->host();
  }

  auto CreateAndWarmGlic() {
    return Do([this] { GetService()->TryPreload(); });
  }

  auto ResetMemoryPressure() {
    return Do([]() {
      GlicProfileManager::ForceMemoryPressureForTesting(
          base::MemoryPressureMonitor::MemoryPressureLevel::
              MEMORY_PRESSURE_LEVEL_NONE);
    });
  }

 private:
  base::test::ScopedFeatureList features_;
};

class GlicApiTestWithOneTabAndContextualCueing : public GlicApiTestWithOneTab {
 public:
  GlicApiTestWithOneTabAndContextualCueing() {
    contextual_cueing_features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlic,
          {
              {"glic-default-hotkey", "Ctrl+G"},
              // Shorten load timeouts.
              {features::kGlicPreLoadingTimeMs.name, "20"},
              {features::kGlicMinLoadingTimeMs.name, "40"},
          }},
         {features::kGlicApiActivationGating, {}},
         {contextual_cueing::kGlicZeroStateSuggestions, {}}},
        /*disabled_features=*/
        {
            features::kGlicWarming,
        });
  }
  // Create the mock service.
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* browser_context) override {
    mock_cueing_service_ = static_cast<
        testing::NiceMock<contextual_cueing::MockContextualCueingService>*>(
        contextual_cueing::ContextualCueingServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                browser_context,
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<testing::NiceMock<
                      contextual_cueing::MockContextualCueingService>>();
                })));

    GlicApiTestWithOneTab::SetUpBrowserContextKeyedServices(browser_context);
  }

  void TearDownOnMainThread() override {
    mock_cueing_service_ = nullptr;
    GlicApiTestWithOneTab::TearDownOnMainThread();
  }

  contextual_cueing::MockContextualCueingService* mock_cueing_service() {
    return mock_cueing_service_.get();
  }

 private:
  raw_ptr<testing::NiceMock<contextual_cueing::MockContextualCueingService>>
      mock_cueing_service_;
  base::test::ScopedFeatureList contextual_cueing_features_;
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
#if defined(SLOW_BINARY)
#define MAYBE_testAllTestsAreRegistered DISABLED_testAllTestsAreRegistered
#else
#define MAYBE_testAllTestsAreRegistered testAllTestsAreRegistered
#endif
IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, MAYBE_testAllTestsAreRegistered) {
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

IN_PROC_BROWSER_TEST_F(GlicApiTest, testLoadWhileWindowClosed) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  window_controller().Close();
  ExecuteJsTest();
  // Make sure the WebUI transitions to kReady, otherwise the web client may be
  // destroyed.
  WaitForWebUiState(mojom::WebUiState::kReady);
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testInitializeFailsWindowClosed) {
  base::HistogramTester histogram_tester;
  // Immediately close the window to check behavior while window is closed.
  // Fail client initialization, should see error page.
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  window_controller().Close();
  ExecuteJsTest();
  WaitForWebUiState(mojom::WebUiState::kError);
  histogram_tester.ExpectUniqueSample("Glic.Host.WebClientState.OnDestroy",
                                      /*WEB_CLIENT_INITIALIZE_FAILED=*/2, 1);
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

// TODO(crbug.com/409042450): This is a flaky on MSAN.
#if defined(SLOW_BINARY)
#define MAYBE_testReload DISABLED_testReload
#else
#define MAYBE_testReload testReload
#endif
IN_PROC_BROWSER_TEST_F(GlicApiTest, MAYBE_testReload) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(&host());
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

IN_PROC_BROWSER_TEST_F(GlicApiTest, testReloadWebUi) {
  WebUIStateListener listener(&host());
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  ExecuteJsTest();

  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  window_controller().Reload();
  listener.WaitForWebUiState(mojom::WebUiState::kUninitialized);
  ExecuteJsTest();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return host().GetPageHandlersForTesting().size() == 1; }));
  // Reloading the WebUI should trigger loading a second page handler.
  // That page handler should become the primary page handler.
  // This assertion is a regression test for b/418258791.
  ASSERT_TRUE(host().GetPrimaryPageHandlerForTesting());
}

// The client navigates to the 'sorry' page before it finishes initialize().
// Chrome should show this page.
IN_PROC_BROWSER_TEST_F(GlicApiTest, testSorryPageBeforeInitialize) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(&host());
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set(
          "failWith", "navigateToSorryPageBeforeInitialize")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kGuestError);
  RunTestSequence(CheckControllerShowing(true));

  // Simulate completing a captcha, navigating back.
  ASSERT_EQ(true,
            content::EvalJs(FindGlicGuestMainFrame(),
                            std::string("(()=>{window.location = '") +
                                GetGuestURL().spec() + "'; return true;})()"));

  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "none")),
  });
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testSorryPageAfterInitialize) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(&host());
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set(
          "failWith", "navigateToSorryPageAfterInitialize")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kGuestError);
  RunTestSequence(CheckControllerShowing(true));

  // Simulate completing a captcha, navigating back.
  ASSERT_EQ(true,
            content::EvalJs(FindGlicGuestMainFrame(),
                            std::string("(()=>{window.location = '") +
                                GetGuestURL().spec() + "'; return true;})()"));

  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "none")),
  });
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testInitializeFailsAfterReload) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(&host());
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

IN_PROC_BROWSER_TEST_F(GlicApiTestWithFastTimeout, testNoClientCreated) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(&host());
  ExecuteJsTest();
  listener.WaitForWebUiState(mojom::WebUiState::kError);
  // Note that the client does receive the bootstrap message, but never calls
  // back, so from the host's perspective bootstrapping is still pending.
  histogram_tester.ExpectUniqueSample("Glic.Host.WebClientState.OnDestroy",
                                      0 /*BOOTSTRAP_PENDING*/, 1);
#endif
}

// In this test, the client page does not initiate the bootstrap process, so no
// client connects.
IN_PROC_BROWSER_TEST_F(GlicApiTestWithFastTimeout, testNoBootstrap) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(&host());
  ExecuteJsTest();
  listener.WaitForWebUiState(mojom::WebUiState::kError);
  histogram_tester.ExpectUniqueSample("Glic.Host.WebClientState.OnDestroy",
                                      0 /*BOOTSTRAP_PENDING*/, 1);
#endif
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithFastTimeout, testInitializeTimesOut) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(&host());
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "timeout")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kError);
  histogram_tester.ExpectUniqueSample("Glic.Host.WebClientState.OnDestroy",
                                      3 /*WEB_CLIENT_NOT_INITIALIZED*/, 1);
#endif
}

// Connect the client, and check that the special request header is sent.
IN_PROC_BROWSER_TEST_F(GlicApiTest, testRequestHeader) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();

  auto main_request = std::ranges::find_if(
      embedded_test_server_requests_, [&](const auto& request) {
        return request.GetURL().path() == GetGuestURL().path();
      });
  ASSERT_NE(main_request, embedded_test_server_requests_.end());
  ASSERT_THAT(
      main_request->headers,
      testing::AllOf(Contains(Pair("x-glic", "1")),
                     Contains(Pair("x-glic-chrome-channel",
                                   testing::AnyOf("unknown", "canary", "dev",
                                                  "beta", "stable"))),
                     Contains(Pair("x-glic-chrome-version",
                                   version_info::GetVersionNumber()))));
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testCreateTab) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckTabCount(1));
  ExecuteJsTest();
  RunTestSequence(CheckTabCount(2));
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testCreateTabFailsWithUnsupportedScheme) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckTabCount(1));
  ExecuteJsTest();
  RunTestSequence(CheckTabCount(1));
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testCreateTabInBackground) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckTabCount(1));

  // Creating a new tab via the glic API in the foreground should change the
  // active tab.
  ExecuteJsTest();
  RunTestSequence(CheckTabCount(2));
  tabs::TabInterface* active_tab =
      InProcessBrowserTest::browser()->tab_strip_model()->GetActiveTab();
  ASSERT_THAT(active_tab->GetContents()->GetURL().spec(),
              testing::EndsWith("#foreground"));

  // Creating a new tab via the glic API in the background should not change the
  // active tab.
  ContinueJsTest();
  RunTestSequence(CheckTabCount(3));
  active_tab =
      InProcessBrowserTest::browser()->tab_strip_model()->GetActiveTab();
  ASSERT_THAT(active_tab->GetContents()->GetURL().spec(),
              testing::EndsWith("#foreground"));
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testCreateTabByClickingOnLink) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckTabCount(1));
  content::RenderFrameHost* guest_frame = FindGlicGuestMainFrame();
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return InProcessBrowserTest::browser()->tab_strip_model()->GetTabCount() ==
           2;
  })) << "Timed out waiting for tab count to increase. Tab count = "
      << InProcessBrowserTest::browser()->tab_strip_model()->GetTabCount();
  // The guest frame shouldn't change.
  ASSERT_EQ(guest_frame, FindGlicGuestMainFrame());

  // This test is a regression test for b/416464184.
  // Audio ducking should still work after clicking a link.
  AudioDucker* audio_ducker =
      AudioDucker::GetForPage(FindGlicGuestMainFrame()->GetPage());
  ASSERT_TRUE(audio_ducker);
  ASSERT_EQ(audio_ducker->GetAudioDuckingState(),
            AudioDucker::AudioDuckingState::kDucking);

  ContinueJsTest();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return audio_ducker->GetAudioDuckingState() ==
           AudioDucker::AudioDuckingState::kNoDucking;
  }));
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testCreateTabFailsIfNotActive) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
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

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testShowProfilePicker) {
  base::test::TestFuture<void> profile_picker_opened;
  ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
      profile_picker_opened.GetCallback());
  ExecuteJsTest();
  ASSERT_TRUE(profile_picker_opened.Wait());
  // TODO(harringtond): Try to test changing profiles.
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testPanelActive) {
  browser_activator().SetMode(BrowserActivator::Mode::kFirst);
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

IN_PROC_BROWSER_TEST_F(GlicApiTest, testIsBrowserOpen) {
  browser_activator().SetMode(BrowserActivator::Mode::kFirst);
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
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return window_controller().GetGlicWidget()->widget_delegate()->CanResize();
  }));
}

// This test is flaky on Mac (crbug.com/414584725).
#if BUILDFLAG(IS_MAC)
#define MAYBE_testDisableDragResize DISABLED_testDisableDragResize
#else
#define MAYBE_testDisableDragResize testDisableDragResize
#endif
IN_PROC_BROWSER_TEST_F(GlicApiTest, MAYBE_testDisableDragResize) {
  // Check the default resize setting here.
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  ExpectUserCanResize(true));
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !window_controller().GetGlicWidget()->widget_delegate()->CanResize();
  }));
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

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTabAndContextualCueing,
                       testGetZeroStateSuggestions) {
  EXPECT_CALL(*mock_cueing_service(),
              GetContextualGlicZeroStateSuggestions(_, _, _))
      .Times(1);

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTabAndContextualCueing,
                       testGetZeroStateSuggestionsFailsWhenHidden) {
  EXPECT_CALL(*mock_cueing_service(),
              GetContextualGlicZeroStateSuggestions(_, _, _))
      .Times(0);

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTabAndPreloading,
                       testDeferredFocusedTabStateAtCreation) {
  // Preload a web contents and then navigate.
  RunTestSequence(
      WaitForShow(kGlicButtonElementId), ResetMemoryPressure(),
      ObserveState(glic::test::internal::kWebUiState, &host()),
      CreateAndWarmGlic(),
      WaitForState(glic::test::internal::kWebUiState,
                   mojom::WebUiState::kReady),
      CheckControllerShowing(false),
      NavigateWebContents(kFirstTab,
                          InProcessBrowserTest::embedded_test_server()->GetURL(
                              "/scrollable_page_with_content.html")));
  ExecuteJsTest();
  RunTestSequence(ToggleGlicWindow(GlicWindowMode::kDetached),
                  CheckControllerShowing(true));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testGetFocusedTabStateV2) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testGetFocusedTabStateV2WithNavigation) {
  // Confirm that the observer is notified through getFocusedTabState of the
  // initial state, i.e. the first page navigation.
  ExecuteJsTest();

  // Navigate to another page in the existing tab.
  RunTestSequence(NavigateWebContents(
      kFirstTab, InProcessBrowserTest::embedded_test_server()->GetURL(
                     "/scrollable_page_with_content.html")));

  // Confirm that the observer is notified through getFocusedTabState of the
  // second page navigation.
  ContinueJsTest();

  // Open a new tab and navigate to a another page.
  RunTestSequence(AddInstrumentedTab(
      kSecondTab,
      InProcessBrowserTest::embedded_test_server()->GetURL("/glic/test.html")));

  // Confirm that the observer is notified through getFocusedTabState that due
  // to a page navigation in a new tab, a new tab has gained focus.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testGetFocusedTabStateV2WithNavigationWhenInactive) {
  // Confirm that the observer is notified through getFocusedTabState of the
  // initial state, i.e. the first page navigation. It should then hide.
  ExecuteJsTest();

  // Navigate to another page in the existing tab.
  RunTestSequence(NavigateWebContents(
      kFirstTab, InProcessBrowserTest::embedded_test_server()->GetURL(
                     "/scrollable_page_with_content.html")));

  // Open a new tab, navigate to a another page, and open the glic window.
  RunTestSequence(
      AddInstrumentedTab(kSecondTab,
                         InProcessBrowserTest::embedded_test_server()->GetURL(
                             "/glic/test.html")),
      OpenGlicWindow(GlicWindowMode::kDetached,
                     GlicInstrumentMode::kHostAndContents));

  // Confirm that the observer only notified of this last state.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testGetFocusedTabStateV2BrowserClosed) {
  browser_activator().SetMode(BrowserActivator::Mode::kFirst);
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

#if BUILDFLAG(ENABLE_PDF)
#define MAYBE_testGetContextFromFocusedTabWithPdfFile \
  testGetContextFromFocusedTabWithPdfFile
#else
#define MAYBE_testGetContextFromFocusedTabWithPdfFile \
  DISABLED_testGetContextFromFocusedTabWithPdfFile
#endif
IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       MAYBE_testGetContextFromFocusedTabWithPdfFile) {
  RunTestSequence(NavigateWebContents(
      kFirstTab,
      InProcessBrowserTest::embedded_test_server()->GetURL("/pdf/test.pdf")));

  ExecuteJsTest();
}

// TODO(harringtond): Fix this, it hangs.
IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, DISABLED_testCaptureScreenshot) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testPermissionAccess) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testClosedCaptioning) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testGetUserProfileInfo) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testGetUserProfileInfoDefersWhenInactive) {
  ExecuteJsTest();
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testRefreshSignInCookies) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testSignInPauseState) {
  // Check that Glic web client is open and can retrieve the user's info.
  ExecuteJsTest({.expect_guest_frame_destroyed = false});

  // Pause the sign-in.
  auto* const identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);

  // The guest frame should be destroyed, and the WebUI should show the sign-in
  // panel.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return FindGlicGuestMainFrame() == nullptr; }));
  WaitForWebUiState(mojom::WebUiState::kSignIn);
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testSetContextAccessIndicator) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testSetAudioDucking) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testGetDisplayMedia) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testMetrics) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testScrollToFindsText) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       testScrollToFindsTextNoTabContextPermission) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testScrollToFailsWhenInactive) {
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

IN_PROC_BROWSER_TEST_F(GlicApiTest, testCloseAndOpenWhileOpening) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  ExecuteJsTest();
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  ContinueJsTest();
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
  g_browser_process->local_state()->SetString(prefs::kGlicLauncherHotkey, "");
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

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testManualResizeChanged) {
  window_controller().GetGlicWidget()->OnNativeWidgetUserResizeStarted();

  // Check that the web client is notified of the beginning of the user
  // initiated resizing event.
  ExecuteJsTest();

  window_controller().GetGlicWidget()->OnNativeWidgetUserResizeEnded();

  // Check that the web client is notified of the ending of the user
  // initiated resizing event.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testResizeWindowTooSmall) {
  // Web client requests the window to be resized to 0x0, bellow the minimum
  // dimensions (see GlicWindowController#GetLastRequestedSizeClamped), so it
  // gets discarded in favor of the initial size.
  gfx::Size expected_size = GlicWidget::GetInitialSize();
  GlicWidget* glic_widget = window_controller().GetGlicWidget();
  ASSERT_TRUE(glic_widget);

  ExecuteJsTest();

  gfx::Rect final_widget_bounds = glic_widget->GetWindowBoundsInScreen();
  ASSERT_EQ(expected_size,
            glic_widget->WidgetToVisibleBounds(final_widget_bounds).size());
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testResizeWindowTooLarge) {
  // Web client requests the window to be resized to 20000x20000, above the
  // maximum dimensions (see GlicWindowController#GetLastRequestedSizeClamped),
  // so it gets discarded in favor of the max size. This max size is still
  // larger than the display work area so we clamp the dimensions down to fit on
  // screen.
  ExecuteJsTest();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  GlicWidget* glic_widget = window_controller().GetGlicWidget();
  ASSERT_TRUE(glic_widget);
  gfx::Rect final_widget_bounds = glic_widget->GetWindowBoundsInScreen();

  ASSERT_TRUE(display_bounds.Contains(final_widget_bounds));
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab, testResizeWindowWithinBounds) {
  // Web client requests the window to be resized to 800x700, which are valid
  // dimensions.
  gfx::Size expected_size = gfx::Size(800, 700);
  ExecuteJsTest(
      {.params = base::Value(base::Value::Dict()
                                 .Set("width", expected_size.width())
                                 .Set("height", expected_size.height()))});
  GlicWidget* glic_widget = window_controller().GetGlicWidget();
  ASSERT_TRUE(glic_widget);
  gfx::Rect final_widget_bounds = glic_widget->GetWindowBoundsInScreen();
  ASSERT_EQ(expected_size,
            glic_widget->WidgetToVisibleBounds(final_widget_bounds).size());
}

class GlicApiTestPageContextEligibilityTest : public GlicApiTest {
 public:
  GlicApiTestPageContextEligibilityTest() {
    eligibility_feature_list_.InitAndEnableFeature(
        features::kGlicPageContextEligibility);
  }

  void SetEligibilityHint(bool is_eligible) {
    optimization_guide::proto::GlicPageContextEligibilityMetadata
        page_context_eligibility_metadata;
    page_context_eligibility_metadata.set_is_eligible(is_eligible);
    optimization_guide::OptimizationMetadata metadata;
    metadata.SetAnyMetadataForTesting(page_context_eligibility_metadata);
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->AddHintForTesting(
            page_url(),
            optimization_guide::proto::GLIC_PAGE_CONTEXT_ELIGIBILITY, metadata);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(optimization_guide::switches::
                                   kDisableCheckingUserPermissionsForTesting);
  }

  GURL page_url() {
    return InProcessBrowserTest::embedded_test_server()->GetURL(
        "/glic/test.html");
  }

 private:
  base::test::ScopedFeatureList eligibility_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicApiTestPageContextEligibilityTest,
                       testGetContextFromFocusedTabWithIneligiblePage) {
  SetEligibilityHint(/*is_eligible=*/false);

  // Load the test page in a tab, so that there is some page context.
  RunTestSequence(InstrumentTab(kFirstTab),
                  NavigateWebContents(kFirstTab, page_url()),
                  OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestPageContextEligibilityTest,
                       testGetContextFromFocusedTabWithEligiblePage) {
  SetEligibilityHint(/*is_eligible=*/true);

  // Load the test page in a tab, so that there is some page context.
  RunTestSequence(InstrumentTab(kFirstTab),
                  NavigateWebContents(kFirstTab, page_url()),
                  OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));

  ExecuteJsTest();
}

class GlicApiTestSystemSettingsTest : public GlicApiTestWithOneTab {
 public:
  GlicApiTestSystemSettingsTest() {
    system_permission_settings::SetInstanceForTesting(&mock_platform_handle);
  }

  ~GlicApiTestSystemSettingsTest() override {
    system_permission_settings::SetInstanceForTesting(nullptr);
  }

  testing::NiceMock<system_permission_settings::MockPlatformHandle>
      mock_platform_handle;
};

IN_PROC_BROWSER_TEST_F(GlicApiTestSystemSettingsTest,
                       testOpenOsMediaPermissionSettings) {
  base::test::TestFuture<void> signal;
  EXPECT_CALL(
      mock_platform_handle,
      OpenSystemSettings(testing::_, ContentSettingsType::MEDIASTREAM_MIC))
      .WillOnce(base::test::InvokeFuture(signal));

  // Trigger the openOsPermissionSettingsMenu API with 'media'.
  ExecuteJsTest();
  // Wait for OpenSystemSettings to be called.
  EXPECT_TRUE(signal.Wait());
}

IN_PROC_BROWSER_TEST_F(GlicApiTestSystemSettingsTest,
                       testOpenOsGeoPermissionSettings) {
  base::test::TestFuture<void> signal;
  EXPECT_CALL(mock_platform_handle,
              OpenSystemSettings(testing::_, ContentSettingsType::GEOLOCATION))
      .WillOnce(base::test::InvokeFuture(signal));

  // Trigger the openOsPermissionSettingsMenu API with 'geolocation'.
  ExecuteJsTest();
  // Wait for OpenSystemSettings to be called.
  EXPECT_TRUE(signal.Wait());
}

IN_PROC_BROWSER_TEST_F(GlicApiTestSystemSettingsTest,
                       testGetOsMicrophonePermissionStatusAllowed) {
  EXPECT_CALL(mock_platform_handle,
              IsAllowed(ContentSettingsType::MEDIASTREAM_MIC))
      .WillOnce(testing::Return(true));

  // Trigger the GetOsMicrophonePermissionStatus API and check if it returns
  // true as mocked by this test.
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestSystemSettingsTest,
                       testGetOsMicrophonePermissionStatusNotAllowed) {
  EXPECT_CALL(mock_platform_handle,
              IsAllowed(ContentSettingsType::MEDIASTREAM_MIC))
      .WillOnce(testing::Return(false));

  // Trigger the GetOsMicrophonePermissionStatus API and check if it returns
  // false as mocked by this test.
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTest, testNavigateToDifferentClientPage) {
  base::HistogramTester histogram_tester;
  WebUIStateListener listener(&host());
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(0)});  // test run count: 0.
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(1)});  // test run count: 1.
  histogram_tester.ExpectUniqueSample("Glic.Host.WebClientState.OnCommit",
                                      6 /*RESPONSIVE*/, 1);
  histogram_tester.ExpectUniqueSample("Glic.Host.WebClientState.OnDestroy",
                                      0 /*BOOTSTRAP_PENDING*/, 1);
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
  WebUIStateListener listener(&host());
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

IN_PROC_BROWSER_TEST_F(GlicApiTest, testCallingApiWhileHiddenRecordsMetrics) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  ExecuteJsTest();
  window_controller().Close();

  base::HistogramTester histogram_tester;
  ContinueJsTest();
  histogram_tester.ExpectBucketCount("Glic.Api.RequestCounts.CreateTab",
                                     GlicRequestEvent::kRequestReceived, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.Api.RequestCounts.CreateTab",
      GlicRequestEvent::kRequestReceivedWhileHidden, 1);
}

}  // namespace
}  // namespace glic

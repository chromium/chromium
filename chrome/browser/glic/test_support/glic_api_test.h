// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_API_TEST_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_API_TEST_H_

#include <type_traits>

#include "base/json/json_writer.h"
#include "base/test/run_until.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

// This file defines Glic API test fixtures NonInteractiveGlicApiTest and
// InteractiveGlicApiTest.
// These fixtures configure a Glic client that runs test code. Each .cc test
// file corresponds to a .ts file. The .cc file runs browser-side code, and the
// .ts file runs glic client code. Each gtest in the .cc file should correspond
// to a test function in the .ts file.
// Using these fixtures requires a little bit of setup. Example:
//
// class MyNewGlicTest : public NonInteractiveGlicApiTest {
//  public:
//   MyNewGlicTest() :
//     // Make a new .ts file in chrome/test/data/webui/glic/browser_tests
//     // update build rules, and point to the generated .js file here.
//     NonInteractiveGlicApiTest("./my_new_glic_test_browsertest.js") {}
// };
//
// // Always include this test in one fixture of your test file. It ensures
// // the set of tests in the .ts file match the set of tests in the .cc file.
// IN_PROC_BROWSER_TEST_F(MyNewGlicTest, testAllTestsAreRegistered) {
//   // Include all test fixture names here.
//   AssertAllTestsRegistered({
//       "MyNewGlicTest",
//   });
// }
// // Add test cases...
//
// Next, here's boilerplate for the my_new_glic_test_browsertest.ts file:
//
// import {ApiTestFixtureBase, testMain} from './browser_test_base.js';
//
// class MyNewGlicTest extends ApiTestFixtureBase {
//   // Normally it's useful to wait until the client is shown to start the test
//   // but is not necessary.
//   override async setUpTest() {
//     await this.client.waitForFirstOpen();
//   }
//   // Add test cases...
// }
// testMain([
//   // All test fixtures need to be listed here.
//   MyNewGlicTest,
// ]);

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

  // Expect that the JS execution should return a failure. Used for internal
  // test harness testing.
  bool should_fail = false;

  // Only considered if `should_fail` is true. This value can be set to the
  // expected string output of the JS error. If this is not set, will only check
  // that the JS result is not "pass".
  std::string_view should_fail_with_error;
};

// Observes the state of the WebUI hosted in the glic window.
class WebUIStateListener : public Host::Observer {
 public:
  explicit WebUIStateListener(Host* host);

  ~WebUIStateListener() override;

  void WebUiStateChanged(mojom::WebUiState state) override;

  // Returns if `state` has been seen. Consumes all observed states up to the
  // point where this state is seen.
  void WaitForWebUiState(mojom::WebUiState state);

 private:
  base::WeakPtr<Host> host_;
  std::deque<mojom::WebUiState> states_;
};

// Observes the state of the WebUI hosted in the glic window.
class CurrentViewListener : public Host::Observer {
 public:
  explicit CurrentViewListener(Host* host);

  ~CurrentViewListener() override;

  void OnViewChanged(mojom::CurrentView view) override;

  // Returns if `state` has been seen. Consumes all observed states up to the
  // point where this state is seen.
  void WaitForCurrentView(mojom::CurrentView view);

 private:
  raw_ptr<Host> host_;
  std::deque<mojom::CurrentView> views_;
};

template <typename T>
  requires std::is_base_of<
      test::InteractiveGlicTestMixin<InteractiveBrowserTest>,
      T>::value
class GlicApiTestBase : public T {
 public:
  template <typename... Args>
  explicit GlicApiTestBase(std::string_view js_source_path, Args&&... args)
      : T(std::forward<Args>(args)...) {
    T::embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &GlicApiTestBase::SorryPageRequestHandler, base::Unretained(this)));
    T::embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &GlicApiTestBase::FakeRpcRequestHandler, base::Unretained(this)));

    T::embedded_test_server()->RegisterRequestMonitor(
        base::BindRepeating(&GlicApiTestBase::OnEmbeddedTestServerHttpRequest,
                            base::Unretained(this)));

    T::add_mock_glic_query_param(
        "test",
        ::testing::UnitTest::GetInstance()->current_test_info()->name());

    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kGlic,
             {
                 {"glic-default-hotkey", "Ctrl+G"},
                 // Shorten load timeouts.
                 {features::kGlicPreLoadingTimeMs.name, "20"},
                 {features::kGlicMinLoadingTimeMs.name, "40"},
             }},
        },
        /*disabled_features=*/
        {
            features::kGlicWarming,
        });

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kGlicHostLogging);
    T::SetGlicPagePath("/glic/browser_tests/test.html");
    T::add_mock_glic_query_param("testsrc", js_source_path);
  }

  ~GlicApiTestBase() override = default;

  void SetUpOnMainThread() override {
    T::host_resolver()->AddRule("a.com", "127.0.0.1");
    T::host_resolver()->AddRule("b.com", "127.0.0.1");
    T::DisableWarming();
    NonInteractiveGlicTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    if (!next_step_required_.empty()) {
      FAIL() << "Test not finished: call ContinueJsTest()";
    }
    NonInteractiveGlicTest::TearDownOnMainThread();
  }

  GlicKeyedService* GetService() {
    Profile* profile = T::browser()->profile();
    return GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  }

  Host* GetHost() {
    GlicInstance* instance = T::GetGlicInstance();
    return instance ? &instance->host() : nullptr;
  }

  // Run the test typescript function. The typescript function must have the
  // same name as the current test.
  // If the test uses `advanceToNextStep()`, then ContinueJsTest() must be
  // called later.
  void ExecuteJsTest(ExecuteTestOptions options = {}) {
    if (options.wait_for_guest) {
      WaitForGuest();
    }
    content::RenderFrameHost* glic_guest_frame = T::FindGlicGuestMainFrame();
    ASSERT_TRUE(glic_guest_frame);
    std::string param_json = base::WriteJson(options.params).value_or("");
    ProcessTestResult(
        glic_guest_frame->GetGlobalId(), options,
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
    content::RenderFrameHost* glic_guest_frame = T::FindGlicGuestMainFrame();
    ASSERT_TRUE(glic_guest_frame);
    ASSERT_TRUE(next_step_required_.contains(glic_guest_frame->GetGlobalId()));
    next_step_required_.erase(glic_guest_frame->GetGlobalId());
    std::string param_json = base::WriteJson(options.params).value_or("");
    ProcessTestResult(
        glic_guest_frame->GetGlobalId(), options,
        content::EvalJs(glic_guest_frame,
                        base::StrCat({"continueApiTest(", param_json, ")"})));
  }

  void WaitForGuest() {
    auto end_time = base::TimeTicks::Now() + base::Seconds(10);
    content::RenderFrameHost* frame = nullptr;
    while (base::TimeTicks::Now() < end_time) {
      // Note: Sometimes the previous guest frame is still around, but it won't
      // have the runApiTest function. Loop until both conditions are met.
      frame = T::FindGlicGuestMainFrame();
      if (frame) {
        auto result =
            content::EvalJs(frame, {"typeof runApiTest !== 'undefined'"});
        if (result.is_ok() && result.ExtractBool()) {
          return;
        }
      }
      sleepWithRunLoop(base::Milliseconds(200));
    }
    FAIL() << "Timed out waiting for guest frame. Guest frame: "
           << (frame ? frame->GetLastCommittedURL().spec() : "not found");
  }

  void WaitForWebUiState(mojom::WebUiState state) {
    WebUIStateListener listener(T::GetHost());
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

  // Fake RPC endpoint that sometimes produces a CORS response.
  // It does not respond to allow preflights, though.
  std::unique_ptr<net::test_server::HttpResponse> FakeRpcRequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.method != net::test_server::METHOD_GET ||
        !base::StartsWith(request.relative_url, "/fake-rpc")) {
      return nullptr;
    }
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_code(net::HttpStatusCode::HTTP_OK);
    result->set_content_type("application/json");
    result->set_content("{\"status\": \"ok\"}");
    if (request.relative_url.find("/cors") != std::string::npos) {
      result->AddCustomHeader("Access-Control-Allow-Origin", "*");
    }
    return result;
  }

  void ProcessTestResult(content::GlobalRenderFrameHostId frame_id,
                         const ExecuteTestOptions& options,
                         const content::EvalJsResult& result) {
    if (options.expect_guest_frame_destroyed) {
      ASSERT_THAT(result, content::EvalJsResult::ErrorIs(
                              testing::HasSubstr("RenderFrame deleted.")));
      return;
    }

    ASSERT_THAT(result, content::EvalJsResult::IsOk());
    if (result.is_dict()) {
      const base::Value::Dict& dict = result.ExtractDict();
      auto* id = dict.Find("id");
      if (id && id->is_string() && id->GetString() == "next-step") {
        step_data_ = dict.Find("payload")->Clone();
      }
      next_step_required_.insert(frame_id);
      return;
    }
    if (!options.should_fail) {
      ASSERT_EQ(result, "pass");
    } else if (options.should_fail_with_error.empty()) {
      ASSERT_NE(result, "pass")
          << "JS step should have failed, but it succeeded";
    } else {
      ASSERT_EQ(result, options.should_fail_with_error)
          << "JS step should have failed, but it succeeded";
    }
  }

  void AssertAllTestsRegistered(
      std::vector<std::string> gunit_test_suite_names) {
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
    GTEST_SKIP() << "AssertAllTestsRegistered not processed for slow binaries.";
#else
    T::RunTestSequence(T::OpenGlicWindow(T::GlicWindowMode::kDetached,
                                         T::GlicInstrumentMode::kNone));
    ExecuteJsTest();
    ASSERT_TRUE(step_data()->is_list());
    ::testing::UnitTest* unit_test = ::testing::UnitTest::GetInstance();
    std::set<std::string> test_suites;
    std::set<std::string> js_test_names, cc_test_names;
    for (const auto& test_name : step_data()->GetList()) {
      js_test_names.insert(test_name.GetString());
    }
    for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
      const auto* test_suite = unit_test->GetTestSuite(i);
      if (!base::Contains(gunit_test_suite_names,
                          std::string(test_suite->name()))) {
        continue;
      }
      for (int j = 0; j < test_suite->total_test_count(); ++j) {
        std::string name = test_suite->GetTestInfo(j)->name();
        // Strips out the test variants suffix.
        name = name.substr(0, name.find_last_of('/'));
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
#endif
  }

  // Records all requests to the embedded test server.
  void OnEmbeddedTestServerHttpRequest(
      const net::test_server::HttpRequest& request) {
    embedded_test_server_requests_.push_back(request);
  }

  void sleepWithRunLoop(base::TimeDelta sleepDuration) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), sleepDuration);
    run_loop.Run();
  }

  std::vector<net::test_server::HttpRequest> embedded_test_server_requests_;
  std::set<content::GlobalRenderFrameHostId> next_step_required_;
  std::optional<base::Value> step_data_;
  base::test::ScopedFeatureList features_;
};

using NonInteractiveGlicApiTest = GlicApiTestBase<NonInteractiveGlicTest>;
using InteractiveGlicApiTest = GlicApiTestBase<test::InteractiveGlicTest>;

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_API_TEST_H_

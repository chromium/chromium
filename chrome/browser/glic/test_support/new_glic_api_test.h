// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_NEW_GLIC_API_TEST_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_NEW_GLIC_API_TEST_H_

#include <sstream>
#include <variant>

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_timeouts.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/common/chrome_switches.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/host/context/glic_focused_browser_manager_impl.h"
#endif

namespace glic {
namespace internal {

struct CloseTabCommand {
  tabs::TabHandle tab_handle;
};

struct ExecJsCommand {
  tabs::TabHandle tab_handle;
  std::string script;
};

struct NavigateTabCommand {
  tabs::TabHandle tab_handle;
  std::string url;
};

struct ParseActionsResultCommand {
  std::string actions_result_base64;
};

struct MakeWaitActionCommand {
  std::optional<base::TimeDelta> duration;
  tabs::TabHandle tab_handle;
  int task_id;
};

struct MakeNavigateActionCommand {
  tabs::TabHandle tab_handle;
  std::string url;
  int task_id;
};

using Command = std::variant<CloseTabCommand,
                             ExecJsCommand,
                             NavigateTabCommand,
                             ParseActionsResultCommand,
                             MakeWaitActionCommand,
                             MakeNavigateActionCommand>;

base::expected<Command, std::string> DeserializeCommand(
    const base::DictValue& dict);

}  // namespace internal

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

  // Explicit instance to use. If null, uses GetOnlyGlicInstance().
  raw_ptr<GlicInstanceImpl> instance = nullptr;

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

  // Returns true if `state` has been seen. Should not be used in combination
  // with `WaitForWebUiState`.
  bool SawState(mojom::WebUiState state) const {
    return std::ranges::contains(states_, state);
  }

 private:
  bool HasState(mojom::WebUiState state);

  base::WeakPtr<Host> host_;
  std::deque<mojom::WebUiState> states_;
};

template <typename T>
class GlicApiBrowserTestMixin : public T {
 private:
  using Base = T;

 public:
  template <typename... Args>
  explicit GlicApiBrowserTestMixin(std::string_view js_source_path,
                                   Args&&... args)
      : Base(std::forward<Args>(args)...) {
    Base::AddMockGlicQueryParam(
        "test",
        ::testing::UnitTest::GetInstance()->current_test_info()->name());

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kGlicHostLogging);
    Base::SetGlicPagePath("/glic/browser_tests/test.html");
    Base::AddMockGlicQueryParam("testsrc", js_source_path);

    Base::embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &GlicApiBrowserTestMixin::OnEmbeddedTestServerHttpRequest,
        base::Unretained(this)));

    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kGlic,
             {
                 {"glic-default-hotkey", "Ctrl+G"},
             }},
            {features::kGlicWebClientLoadTimes,
             {
                 // Shorten transition times.
                 {features::kGlicPreLoadingTimeMs.name, "20"},
                 {features::kGlicMinLoadingTimeMs.name, "40"},
                 // Lengthen max loading time.
                 {features::kGlicMaxLoadingTimeMs.name, "30000"},
             }},
        },
        /*disabled_features=*/
        {
            features::kGlicWarming,
            // Tests are sometimes slow, and fail the responsiveness check.
            features::kGlicClientResponsivenessCheck,
        });
  }
  ~GlicApiBrowserTestMixin() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    Base::SetUpCommandLine(command_line);
    // Suppress background activity.
    command_line->AppendSwitch(switches::kDisableBackgroundNetworking);
    command_line->AppendSwitch(switches::kDisableComponentUpdate);
    command_line->AppendSwitch(switches::kDisableDefaultApps);
    // Ensure our background web contents stays awake.
    // TODO(b/495451913): This shouldn't be necessary.
    command_line->AppendSwitch(switches::kDisableRendererBackgrounding);
    command_line->AppendSwitch(switches::kDisableBackgroundTimerThrottling);
  }

  void SetUpOnMainThread() override {
    Base::SetUpOnMainThread();
#if !BUILDFLAG(IS_ANDROID)
    // Makes active browser selection deterministic for tests.
    GlicFocusedBrowserManagerImpl::SetTestingModeForTesting(true);
#endif
  }

  content::RenderFrameHost* FindGlicGuestMainFrame(
      GlicInstanceImpl* instance = nullptr) {
    if (!instance) {
      instance = this->GetOnlyGlicInstance();
    }
    if (!instance) {
      return nullptr;
    }
    return instance->host().GetGuestMainFrame();
  }

  // Run the test typescript function. The typescript function must have the
  // same name as the current test.
  // If the test uses `advanceToNextStep()`, then ContinueJsTest() must be
  // called later.
  void ExecuteJsTest(ExecuteTestOptions options = {}) {
    if (options.wait_for_guest) {
      ASSERT_OK(WaitForGuest(options.instance));
    }
    content::RenderFrameHost* glic_guest_frame =
        FindGlicGuestMainFrame(options.instance);
    ASSERT_TRUE(glic_guest_frame);
    std::string param_json = base::WriteJson(options.params).value_or("");
    std::string test_init_data =
        base::WriteJson(base::DictValue().Set(
                            "embeddedTestServerUrl",
                            Base::embedded_test_server()->GetURL("/").spec()))
            .value_or("");
    ProcessTestResult(
        glic_guest_frame->GetGlobalId(), options,
        content::EvalJs(
            glic_guest_frame,
            base::StrCat(
                {"runApiTest(",
                 base::NumberToString((TestTimeouts::action_max_timeout() * 0.9)
                                          .InMilliseconds()),
                 ",", param_json, ",", test_init_data, ")"})));
  }

  // Continues test execution if `advanceToNextStep()` was used to return
  // control to C++.
  void ContinueJsTest(ExecuteTestOptions options = {}) {
    content::RenderFrameHost* glic_guest_frame =
        FindGlicGuestMainFrame(options.instance);
    ASSERT_TRUE(glic_guest_frame);
    ASSERT_TRUE(next_step_required_.contains(glic_guest_frame->GetGlobalId()));
    next_step_required_.erase(glic_guest_frame->GetGlobalId());
    std::string param_json = base::WriteJson(options.params).value_or("");
    ProcessTestResult(
        glic_guest_frame->GetGlobalId(), options,
        content::EvalJs(glic_guest_frame,
                        base::StrCat({"continueApiTest(", param_json, ")"})));
  }

  [[nodiscard]] TestResult<content::RenderFrameHost*> WaitForGuest(
      GlicInstanceImpl* instance = nullptr) {
    auto end_time = base::TimeTicks::Now() + base::Seconds(30);
    auto next_message_time = base::TimeTicks::Now() + base::Seconds(2);
    auto sleep_time = base::Milliseconds(200);
    content::RenderFrameHost* frame = nullptr;
    content::RenderFrameHost* last_frame = nullptr;
    while (base::TimeTicks::Now() < end_time) {
      // Note: Sometimes the previous guest frame is still around, but it won't
      // have the runApiTest function. Loop until both conditions are met.
      frame = FindGlicGuestMainFrame(instance);
      if (frame) {
#if !BUILDFLAG(IS_ANDROID)
        if (frame != last_frame) {
          frame->GetProcess()->SetPriorityOverride(
              base::Process::Priority::kUserVisible);
        }
#else
        (void)last_frame;  // Unused on Android.
#endif
        last_frame = frame;
        auto result =
            content::EvalJs(frame, {"typeof runApiTest !== 'undefined'"});
        if (result.is_ok() && result.ExtractBool()) {
          return base::ok(frame);
        }
      }
      if (base::TimeTicks::Now() > next_message_time) {
        LOG(INFO) << "Waiting for guest frame. Guest frame: "
                  << (frame ? frame->GetLastCommittedURL().spec()
                            : "not found");
        next_message_time = base::TimeTicks::Now() + base::Seconds(2);
      }
      sleepWithRunLoop(sleep_time);
      sleep_time = std::min(base::Seconds(2), 2 * sleep_time);
    }
    return base::unexpected(base::StrCat(
        {"Timed out waiting for guest frame. Guest frame: ",
         (frame ? frame->GetLastCommittedURL().spec() : "not found")}));
  }

  void sleepWithRunLoop(base::TimeDelta duration) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), duration);
    run_loop.Run();
  }

  void AssertAllTestsRegistered(
      std::vector<std::string> gunit_test_suite_names) {
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
    GTEST_SKIP() << "AssertAllTestsRegistered not processed for slow binaries.";
#else
    ASSERT_OK(Base::OpenGlicForActiveTab());
    ExecuteJsTest();
    ASSERT_TRUE(step_data());
    ASSERT_TRUE(step_data()->is_list());
    ::testing::UnitTest* unit_test = ::testing::UnitTest::GetInstance();
    std::set<std::string> test_suites;
    std::set<std::string> js_test_names, cc_test_names;
    for (const auto& test_name : step_data()->GetList()) {
      js_test_names.insert(test_name.GetString());
    }
    for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
      const auto* test_suite = unit_test->GetTestSuite(i);
      if (!std::ranges::contains(gunit_test_suite_names,
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

  const std::optional<base::Value>& step_data() const { return step_data_; }

 private:
  void OnEmbeddedTestServerHttpRequest(
      const net::test_server::HttpRequest& request) {
    VLOG(1) << "EmbeddedTestServerHttpRequest: " << request.relative_url;
  }
  void ProcessTestResult(content::GlobalRenderFrameHostId frame_id,
                         const ExecuteTestOptions& options,
                         const content::EvalJsResult& result_in) {
    auto result = result_in;
    for (;;) {
      if (options.expect_guest_frame_destroyed) {
        ASSERT_THAT(result, content::EvalJsResult::ErrorIs(
                                testing::HasSubstr("RenderFrame deleted.")));
        return;
      }

      ASSERT_TRUE(result.is_ok());
      if (result.is_dict()) {
        content::EvalJsResult result_copy = result;
        const base::DictValue& dict = result_copy.ExtractDict();
        auto* id = dict.Find("id");
        if (id && id->is_string() && id->GetString() == "next-step") {
          step_data_ = dict.Find("payload")->Clone();
          next_step_required_.insert(frame_id);
          return;
        }
        auto* command = dict.Find("command");
        if (command) {
          auto deserialized = internal::DeserializeCommand(dict);
          if (!deserialized.has_value()) {
            FAIL() << "DeserializeCommand failed: " << deserialized.error();
          }
          base::expected<base::Value, std::string> command_result =
              ProcessCommand(*deserialized);
          if (!command_result.has_value()) {
            FAIL() << "ProcessCommand failed: " << command_result.error();
          }

          content::RenderFrameHost* glic_guest_frame =
              options.instance ? options.instance->host().GetGuestMainFrame()
                               : FindGlicGuestMainFrame(options.instance);
          ASSERT_TRUE(glic_guest_frame);
          std::string result_json =
              base::WriteJson(*command_result).value_or("null");
          result = content::EvalJs(
              glic_guest_frame,
              base::StrCat({"continueApiTest(", result_json, ")"}));
          continue;
        }
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
      return;
    }
  }

  base::expected<base::Value, std::string> ProcessCommand(
      const internal::Command& command) {
    return std::visit(
        absl::Overload{[this](const internal::CloseTabCommand& cmd) {
                         return CommandCloseTab(cmd);
                       },
                       [this](const internal::ExecJsCommand& cmd) {
                         return CommandExecJs(cmd);
                       },
                       [this](const internal::NavigateTabCommand& cmd) {
                         return CommandNavigateTab(cmd);
                       },
                       [this](const internal::ParseActionsResultCommand& cmd) {
                         return CommandParseActionsResult(cmd);
                       },
                       [this](const internal::MakeWaitActionCommand& cmd) {
                         return CommandMakeWaitAction(cmd);
                       },
                       [this](const internal::MakeNavigateActionCommand& cmd) {
                         return CommandMakeNavigateAction(cmd);
                       }},
        command);
  }

  base::expected<base::Value, std::string> CommandCloseTab(
      const internal::CloseTabCommand& command) {
    T::GetTabListInterface()->CloseTab(command.tab_handle);
    return base::ok(base::Value(true));
  }

  base::expected<base::Value, std::string> CommandExecJs(
      const internal::ExecJsCommand& command) {
    bool ok = content::ExecJs(command.tab_handle.Get()->GetContents(),
                              command.script);
    return base::ok(base::Value(ok));
  }

  base::expected<base::Value, std::string> CommandNavigateTab(
      const internal::NavigateTabCommand& command) {
    auto handle = command.tab_handle;
    if (handle == tabs::TabHandle::Null()) {
      if (!T::GetTabListInterface()->GetActiveTab()) {
        return base::unexpected("NavigateTab(): No active tab found");
      }
      handle = T::GetTabListInterface()->GetActiveTab()->GetHandle();
    }
    bool ok =
        content::NavigateToURL(handle.Get()->GetContents(), GURL(command.url));
    return base::ok(base::Value(ok));
  }

  base::expected<base::Value, std::string> CommandParseActionsResult(
      const internal::ParseActionsResultCommand& command) {
    auto result =
        ::actor::ParseBase64Proto<optimization_guide::proto::ActionsResult>(
            command.actions_result_base64);
    if (!result.has_value()) {
      return base::unexpected(result.error());
    }
    std::ostringstream oss;
    oss << static_cast<::actor::mojom::ActionResultCode>(
        result->action_result());
    return base::ok(base::Value(oss.str()));
  }

  base::expected<base::Value, std::string> CommandMakeWaitAction(
      const internal::MakeWaitActionCommand& command) {
    auto handle = command.tab_handle;
    if (handle == tabs::TabHandle::Null()) {
      if (!T::GetTabListInterface()->GetActiveTab()) {
        return base::unexpected("MakeWaitAction(): No active tab found");
      }
      handle = T::GetTabListInterface()->GetActiveTab()->GetHandle();
    }
    optimization_guide::proto::Actions actions = ::actor::MakeWait(
        command.duration, handle, ::actor::TaskId(command.task_id));
    std::string serialized;
    CHECK(actions.SerializeToString(&serialized));
    return base::ok(base::Value(base::Base64Encode(serialized)));
  }

  base::expected<base::Value, std::string> CommandMakeNavigateAction(
      const internal::MakeNavigateActionCommand& command) {
    auto handle = command.tab_handle;
    if (handle == tabs::TabHandle::Null()) {
      if (!T::GetTabListInterface()->GetActiveTab()) {
        return base::unexpected("MakeNavigateAction(): No active tab found");
      }
      handle = T::GetTabListInterface()->GetActiveTab()->GetHandle();
    }
    optimization_guide::proto::Actions actions = ::actor::MakeNavigate(
        handle, command.url, ::actor::TaskId(command.task_id));
    std::string serialized;
    CHECK(actions.SerializeToString(&serialized));
    return base::ok(base::Value(base::Base64Encode(serialized)));
  }

  base::test::ScopedFeatureList features_;
  std::set<content::GlobalRenderFrameHostId> next_step_required_;
  std::optional<base::Value> step_data_;
};

using GlicApiBrowserTest = GlicApiBrowserTestMixin<GlicBrowserTest>;
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_NEW_GLIC_API_TEST_H_

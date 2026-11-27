// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/execution_engine_dispatcher.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/actor/tool_result_data.h"
#include "chrome/browser/actor/tools/fake_tool_request.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tool_result.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {

namespace {

using ToolRequestPtr = std::unique_ptr<ToolRequest>;

using StepCallback =
    base::RepeatingCallback<ExecutionEngineDispatcher::NextAction(
        std::optional<ToolResultWithLatencyInfo> previous_result)>;

// A fake dispatcher in which tests can specify the tools and result to return
// from the dispatcher's GetNext method.
class FakeDispatcher : public ExecutionEngineDispatcher {
 public:
  FakeDispatcher(std::vector<StepCallback> steps, std::string_view name)
      : steps_(std::move(steps)), name_(name) {
    CHECK(!steps_.empty());
  }
  ~FakeDispatcher() override = default;

  NextAction GetNext(ToolDelegate& tool_delegate) override {
    CHECK(!awaiting_tool_completion_);
    CHECK(!finished_);
    CHECK_LT(next_step_index_, steps_.size());

    std::optional<ToolResultWithLatencyInfo> previous_result =
        std::exchange(previous_result_, std::nullopt);
    NextAction next =
        steps_[next_step_index_++].Run(std::move(previous_result));
    if (std::holds_alternative<ToolRequestPtr>(next)) {
      awaiting_tool_completion_ = true;
    } else {
      auto& result = std::get<ToolResultWithLatencyInfo>(next);
      if (result.tool_name.empty()) {
        result.tool_name = name_;
      }
      finished_ = true;
    }
    return next;
  }

  void OnToolCompleted(ToolResultWithLatencyInfo result) override {
    ASSERT_TRUE(awaiting_tool_completion_);
    ASSERT_FALSE(finished_);
    awaiting_tool_completion_ = false;
    previous_result_ = std::move(result);
  }

 private:
  std::vector<StepCallback> steps_;
  std::string name_;
  size_t next_step_index_ = 0;
  bool awaiting_tool_completion_ = false;
  bool finished_ = false;
  std::optional<ToolResultWithLatencyInfo> previous_result_;
};

// A ToolRequest which returns the above fake Dispatcher to return the given
// `steps` from the dispatcher's GetNext method.
class FakeCompositeToolRequest : public ToolRequest {
 public:
  explicit FakeCompositeToolRequest(std::vector<StepCallback> steps,
                                    std::string_view name,
                                    ActorSurfaceHandle actor_surface_handle)
      : steps_(std::move(steps)),
        name_(name),
        actor_surface_handle_(actor_surface_handle) {}
  ~FakeCompositeToolRequest() override = default;

  std::unique_ptr<ExecutionEngineDispatcher> CreateDispatcher() const override {
    return std::make_unique<FakeDispatcher>(std::move(steps_), name_);
  }

  CreateToolResult CreateTool(TaskId, ToolDelegate&) const override {
    NOTREACHED();
  }

  void Apply(ToolRequestVisitorFunctor&) const override { NOTREACHED(); }

  std::string_view Name() const override { return name_; }

  ActorSurfaceHandle GetActorSurfaceHandle() const override {
    return actor_surface_handle_;
  }

 private:
  std::vector<StepCallback> steps_;
  std::string name_;
  ActorSurfaceHandle actor_surface_handle_;
};

ToolRequestPtr MakeFakeCompositeToolRequest(
    std::vector<StepCallback> steps,
    std::string_view name = "FakeCompositeTool",
    ActorSurfaceHandle actor_surface_handle = ActorSurfaceHandle::Null()) {
  return std::make_unique<FakeCompositeToolRequest>(std::move(steps), name,
                                                    actor_surface_handle);
}

ToolResultWithLatencyInfo MakeCompositeResult(
    ToolResult tool_result,
    std::string_view tool_name = "FakeCompositeTool") {
  const base::TimeTicks now = base::TimeTicks::Now();
  return ToolResultWithLatencyInfo(now, now, std::move(tool_result), tool_name);
}

// A fake tool request that records a label into an execution log when invoked
// so tests can verify sub-tool invocation and ordering without running real
// tools.
class LoggingToolRequest : public FakeToolRequest {
 public:
  // Appends `label` to `execution_log` when the created tool is invoked, then
  // immediately completes the tool with `result`. `execution_log` must outlive
  // the tool's invocation.
  LoggingToolRequest(
      std::vector<std::string>& execution_log,
      std::string label,
      mojom::ActionResultPtr result =
          MakeOkResult(/*requires_page_stabilization=*/false),
      ActorSurfaceHandle actor_surface_handle = ActorSurfaceHandle::Null())
      : FakeToolRequest(base::BindOnce(
                            [](std::vector<std::string>* execution_log,
                               std::string label,
                               mojom::ActionResultPtr result,
                               DeprecatedToolCallback callback) {
                              execution_log->push_back(std::move(label));
                              std::move(callback).Run(std::move(result));
                            },
                            &execution_log,
                            std::move(label),
                            std::move(result)),
                        base::OnceClosure(),
                        actor_surface_handle) {}
  ~LoggingToolRequest() override = default;
};

// The "StepCallbacks" below are helper wrappers to wrap and return a given
// Tool or ToolResult into a lambda usable from MakeFakeCompositeToolRequest to
// return the Tool/Result from the Dispatcher's GetNext method.

StepCallback Step(ToolRequestPtr request) {
  return base::BindRepeating(
      [](ToolRequestPtr& request, std::optional<ToolResultWithLatencyInfo>)
          -> ExecutionEngineDispatcher::NextAction {
        CHECK(request);
        return std::move(request);
      },
      base::OwnedRef(std::move(request)));
}

StepCallback StepLoggingToolRequest(
    std::vector<std::string>& execution_log,
    std::string label,
    mojom::ActionResultPtr result =
        MakeOkResult(/*requires_page_stabilization=*/false),
    ActorSurfaceHandle actor_surface_handle = ActorSurfaceHandle::Null()) {
  return base::BindRepeating(
      [](std::vector<std::string>* execution_log, const std::string& label,
         mojom::ActionResultPtr& result,
         ActorSurfaceHandle actor_surface_handle,
         std::optional<ToolResultWithLatencyInfo>)
          -> ExecutionEngineDispatcher::NextAction {
        CHECK(result);
        return std::make_unique<LoggingToolRequest>(
            *execution_log, label, std::move(result), actor_surface_handle);
      },
      &execution_log, std::move(label), base::OwnedRef(std::move(result)),
      actor_surface_handle);
}

StepCallback StepLoggingToolRequest(
    std::vector<std::string>& execution_log,
    std::string label,
    mojom::ActionResultCode result_code,
    ActorSurfaceHandle actor_surface_handle = ActorSurfaceHandle::Null()) {
  mojom::ActionResultPtr result =
      IsOk(result_code)
          ? MakeOkResult(/*requires_page_stabilization=*/false)
          : MakeResult(result_code, /*requires_page_stabilization=*/false);
  return StepLoggingToolRequest(execution_log, std::move(label),
                                std::move(result), actor_surface_handle);
}

StepCallback ReturnResult(ToolResult tool_result,
                          std::string_view tool_name = {}) {
  return base::BindRepeating(
      [](const ToolResult& tool_result, const std::string& tool_name,
         std::optional<ToolResultWithLatencyInfo>)
          -> ExecutionEngineDispatcher::NextAction {
        return MakeCompositeResult(tool_result.Clone(), tool_name);
      },
      std::move(tool_result), std::string(tool_name));
}

StepCallback StepInvokeTestFuture(
    base::test::TestFuture<DeprecatedToolCallback>& invoke_future) {
  return base::BindLambdaForTesting(
      [&invoke_future](std::optional<ToolResultWithLatencyInfo>)
          -> ExecutionEngineDispatcher::NextAction {
        return std::make_unique<FakeToolRequest>(invoke_future.GetCallback(),
                                                 base::OnceClosure());
      });
}

StepCallback ExpectStepNotCalled() {
  return base::BindRepeating([](std::optional<ToolResultWithLatencyInfo>)
                                 -> ExecutionEngineDispatcher::NextAction {
    ADD_FAILURE() << "Unexpected GetNext call on dispatcher.";
    return ToolRequestPtr(nullptr);
  });
}

StepCallback StepFakeCompositeToolRequest(
    std::vector<StepCallback> steps,
    std::string_view name = "FakeCompositeTool",
    ActorSurfaceHandle actor_surface_handle = ActorSurfaceHandle::Null()) {
  return Step(MakeFakeCompositeToolRequest(std::move(steps), name,
                                           actor_surface_handle));
}

// Returns a gmock action for a UiEventDispatcher method that completes with
// `code`.
template <typename T>
auto UiEventResult(mojom::ActionResultCode code) {
  return UiEventDispatcherCallback<T>(base::BindRepeating(
      [](mojom::ActionResultCode code) {
        return IsOk(code)
                   ? MakeOkResult(/*requires_page_stabilization=*/false)
                   : MakeResult(code, /*requires_page_stabilization=*/false);
      },
      code));
}

// Returns a mock UiEventDispatcher where every event succeeds by default.
// Tests can use EXPECT_CALL to make individual events fail.
std::unique_ptr<testing::NiceMock<ui::MockUiEventDispatcher>>
MakeSucceedingMockUiEventDispatcher() {
  auto dispatcher =
      std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
  ON_CALL(*dispatcher, OnPreTool)
      .WillByDefault(UiEventResult<ToolRequest>(mojom::ActionResultCode::kOk));
  ON_CALL(*dispatcher, OnPostTool)
      .WillByDefault(UiEventResult<ToolRequest>(mojom::ActionResultCode::kOk));
  ON_CALL(*dispatcher, OnActorTaskAsyncChange)
      .WillByDefault(UiEventResult<ui::UiEventDispatcher::ActorTaskAsyncChange>(
          mojom::ActionResultCode::kOk));
  return dispatcher;
}

class ExecutionEngineDispatcherBrowserTest : public InProcessBrowserTest {
 public:
  ExecutionEngineDispatcherBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlic, {}},
                              {kGlicCrossOriginNavigationGating,
                               {{"confirm_navigation_to_new_origins",
                                 "false"}}}},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ExecutionEngineDispatcherBrowserTest(
      const ExecutionEngineDispatcherBrowserTest&) = delete;
  ExecutionEngineDispatcherBrowserTest& operator=(
      const ExecutionEngineDispatcherBrowserTest&) = delete;
  ~ExecutionEngineDispatcherBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "components/test/data");
    ASSERT_TRUE(embedded_https_test_server().Start());

    task_id_ = actor_keyed_service()->CreateTask(actor::TestTaskSourceInfo(),
                                                 NoEnterprisePolicyChecker());
  }

 protected:
  tabs::TabInterface* active_tab() {
    return browser()->tab_strip_model()->GetActiveTab();
  }

  content::WebContents* web_contents() { return active_tab()->GetContents(); }

  ActorKeyedService* actor_keyed_service() {
    return ActorKeyedService::Get(browser()->GetProfile());
  }

  ActorTask& actor_task() { return *actor_keyed_service()->GetTask(task_id_); }

  // Creates a new task which uses `ui_event_dispatcher` rather than the real
  // UI.
  ActorTask& CreateTaskWithUiEventDispatcher(
      std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher) {
    return *actor_keyed_service()->GetTask(
        actor_keyed_service()->CreateTaskForTesting(
            std::move(ui_event_dispatcher), TestTaskSourceInfo(),
            NoEnterprisePolicyChecker()));
  }

 private:
  TaskId task_id_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that a composite tool request pushes its dispatcher onto the stack,
// executes multiple sub-tools in order, and produces a single top-level result.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       CompositeToolExecutesMultipleActions) {
  std::vector<std::string> execution_log;

  ToolRequestPtr composite = MakeFakeCompositeToolRequest({
      StepLoggingToolRequest(execution_log, "step_1"),
      StepLoggingToolRequest(execution_log, "step_2"),
      ReturnResult(ToolResult::Success()),
  });

  ActResultFuture result;
  actor_task().Act(ToRequestList(composite), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_THAT(execution_log, testing::ElementsAre("step_1", "step_2"));
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_name, "FakeCompositeTool");
}

// Verifies that a composite tool executes in order between regular tool
// requests in a sequence and that execution resumes on the parent dispatcher.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       CompositeToolInSequenceWithRegularActions) {
  std::vector<std::string> execution_log;

  ToolRequestPtr before =
      std::make_unique<LoggingToolRequest>(execution_log, "before");
  ToolRequestPtr composite = MakeFakeCompositeToolRequest({
      StepLoggingToolRequest(execution_log, "composite_1"),
      StepLoggingToolRequest(execution_log, "composite_2"),
      ReturnResult(ToolResult::Success()),
  });
  ToolRequestPtr after =
      std::make_unique<LoggingToolRequest>(execution_log, "after");

  ActResultFuture result;
  actor_task().Act(ToRequestList(before, composite, after),
                   result.GetCallback());
  ExpectOkResult(result);

  EXPECT_THAT(execution_log, testing::ElementsAre("before", "composite_1",
                                                  "composite_2", "after"));
  ASSERT_EQ(result.Get().size(), 3u);
  EXPECT_EQ(result.Get()[0].tool_name, "FakeTool");
  EXPECT_EQ(result.Get()[1].tool_name, "FakeCompositeTool");
  EXPECT_EQ(result.Get()[2].tool_name, "FakeTool");
}

// Verifies that a composite dispatcher can yield another composite tool
// request, pushing a nested dispatcher onto the stack and resuming the outer
// dispatcher when completed.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       NestedCompositeTools) {
  std::vector<std::string> execution_log;

  ToolRequestPtr outer_composite = MakeFakeCompositeToolRequest(
      {
          StepLoggingToolRequest(execution_log, "outer_1"),
          StepFakeCompositeToolRequest(
              {
                  StepLoggingToolRequest(execution_log, "inner_1"),
                  StepLoggingToolRequest(execution_log, "inner_2"),
                  ReturnResult(ToolResult::Success()),
              },
              "InnerComposite"),
          StepLoggingToolRequest(execution_log, "outer_2"),
          ReturnResult(ToolResult::Success()),
      },
      "OuterComposite");

  ActResultFuture result;
  actor_task().Act(ToRequestList(outer_composite), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_THAT(execution_log,
              testing::ElementsAre("outer_1", "inner_1", "inner_2", "outer_2"));
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_name, "OuterComposite");
}

// Verifies that each step in a composite dispatcher is requested lazily after
// the preceding sub-tool has finished executing.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       StepsGeneratedLazilyAfterPriorStepCompletes) {
  std::vector<std::string> execution_log;
  ToolRequestPtr second_step_tool;

  ToolRequestPtr composite = MakeFakeCompositeToolRequest({
      base::BindLambdaForTesting([&](std::optional<ToolResultWithLatencyInfo>)
                                     -> ExecutionEngineDispatcher::NextAction {
        return std::make_unique<FakeToolRequest>(
            base::BindLambdaForTesting([&](DeprecatedToolCallback callback) {
              // When invoked, this tool will generate a second tool call
              // usable by the generator's next step. This ensures tools can be
              // created at execution time.
              second_step_tool = std::make_unique<LoggingToolRequest>(
                  execution_log, "second_tool");
              execution_log.push_back("first_tool");
              std::move(callback).Run(
                  MakeOkResult(/*requires_page_stabilization=*/false));
            }),
            base::OnceClosure());
      }),
      base::BindLambdaForTesting([&](std::optional<ToolResultWithLatencyInfo>)
                                     -> ExecutionEngineDispatcher::NextAction {
        CHECK(second_step_tool);
        return std::move(second_step_tool);
      }),
      ReturnResult(ToolResult::Success()),
  });

  ActResultFuture result;
  actor_task().Act(ToRequestList(composite), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_THAT(execution_log, testing::ElementsAre("first_tool", "second_tool"));
  ASSERT_EQ(result.Get().size(), 1u);
}

struct FakeStepResultData : public ToolResultData {
  explicit FakeStepResultData(int value) : value(value) {}
  FakeStepResultData(const FakeStepResultData&) = default;
  FakeStepResultData& operator=(const FakeStepResultData&) = default;
  ~FakeStepResultData() override = default;

  base::DictValue ToDict() const override {
    return base::DictValue().Set("value", value);
  }
  std::unique_ptr<ToolResultData> Clone() const override {
    return std::make_unique<FakeStepResultData>(*this);
  }

  int value = 0;
};

// Verifies that typed ToolResultData from each sub-tool is passed to the
// dispatcher via OnToolCompleted and is not leaked onto the composite tool's
// own result.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       SubToolResultDataPassedToDispatcher) {
  int sum = 0;

  ToolRequestPtr composite = MakeFakeCompositeToolRequest({
      base::BindLambdaForTesting([](std::optional<ToolResultWithLatencyInfo>)
                                     -> ExecutionEngineDispatcher::NextAction {
        return std::make_unique<FakeToolRequest>(
            ToolResult::Success(FakeStepResultData(21)));
      }),
      base::BindLambdaForTesting([](std::optional<ToolResultWithLatencyInfo>
                                        prev_result)
                                     -> ExecutionEngineDispatcher::NextAction {
        CHECK(prev_result->tool_result.data());
        int prev =
            prev_result->tool_result.data()->As<FakeStepResultData>()->value;
        return std::make_unique<FakeToolRequest>(
            ToolResult::Success(FakeStepResultData(prev * 2)));
      }),
      base::BindLambdaForTesting([&](std::optional<ToolResultWithLatencyInfo>
                                         prev_result)
                                     -> ExecutionEngineDispatcher::NextAction {
        CHECK(prev_result->tool_result.data());
        int final_val =
            prev_result->tool_result.data()->As<FakeStepResultData>()->value;
        sum = final_val + 1;
        return std::make_unique<FakeToolRequest>(ToolResult::Success());
      }),
      ReturnResult(ToolResult::Success()),
  });

  ActResultFuture result;
  actor_task().Act(ToRequestList(composite), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(sum, 43);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_result.data(), nullptr);
}

// Verifies that ToolResultData on the ToolResult returned by a composite tool
// itself is preserved in the final Act() result.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       CompositeToolResultDataReturnedToCaller) {
  std::vector<std::string> execution_log;

  ToolRequestPtr composite = MakeFakeCompositeToolRequest({
      StepLoggingToolRequest(execution_log, "step_1"),
      ReturnResult(ToolResult::Success(FakeStepResultData(99))),
  });

  ActResultFuture result;
  actor_task().Act(ToRequestList(composite), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_THAT(execution_log, testing::ElementsAre("step_1"));
  ASSERT_EQ(result.Get().size(), 1u);
  ASSERT_NE(result.Get()[0].tool_result.data(), nullptr);
  EXPECT_EQ(result.Get()[0].tool_result.data()->As<FakeStepResultData>()->value,
            99);
}

// Verifies that an unhandled sub-tool error returned by a nested composite
// dispatcher unwinds multiple dispatcher nestings and aborts the sequence.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       SubToolFailureUnwindsStackAndStopsSequence) {
  std::vector<std::string> execution_log;

  const auto error_code = mojom::ActionResultCode::kElementOffscreen;
  ToolRequestPtr outer_composite = MakeFakeCompositeToolRequest(
      {
          StepLoggingToolRequest(execution_log, "outer_before"),
          StepFakeCompositeToolRequest(
              {
                  StepLoggingToolRequest(
                      execution_log, "failing_sub_tool",
                      mojom::ActionResultCode::kElementOffscreen),
                  ReturnResult(ToolResult::Error(ToolError(error_code)),
                               "InnerComposite"),
                  ExpectStepNotCalled(),
              },
              "InnerComposite"),
          ReturnResult(ToolResult::Error(ToolError(error_code)),
                       "OuterComposite"),
          ExpectStepNotCalled(),
      },
      "OuterComposite");

  ToolRequestPtr trailing =
      std::make_unique<LoggingToolRequest>(execution_log, "trailing");

  ActResultFuture result;
  actor_task().Act(ToRequestList(outer_composite, trailing),
                   result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kElementOffscreen);

  EXPECT_THAT(execution_log,
              testing::ElementsAre("outer_before", "failing_sub_tool"));
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_name, "OuterComposite");
}

// Verifies that a nested dispatcher can observe a sub-tool failure and issue a
// fallback sub-tool that succeeds, allowing the dispatcher and Act() to
// succeed.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       NestedDispatcherHandlesSubToolFailureWithFallback) {
  std::vector<std::string> execution_log;

  ToolRequestPtr composite = MakeFakeCompositeToolRequest({
      StepLoggingToolRequest(execution_log, "failing_tool",
                             mojom::ActionResultCode::kElementOffscreen),
      StepLoggingToolRequest(execution_log, "fallback_tool",
                             mojom::ActionResultCode::kOk),
      ReturnResult(ToolResult::Success()),
  });

  ActResultFuture result;
  actor_task().Act(ToRequestList(composite), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_THAT(execution_log,
              testing::ElementsAre("failing_tool", "fallback_tool"));
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_name, "FakeCompositeTool");
}

// Verifies that a nested dispatcher can recover from a sub-tool that fails in
// the UI pre-invoke stage (i.e. after the tool was created and validated but
// before it was invoked) by issuing a fallback sub-tool that succeeds.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       NestedDispatcherHandlesUiPreInvokeFailureWithFallback) {
  std::vector<std::string> execution_log;

  auto ui_event_dispatcher = MakeSucceedingMockUiEventDispatcher();
  EXPECT_CALL(*ui_event_dispatcher, OnPreTool)
      .WillOnce(
          UiEventResult<ToolRequest>(mojom::ActionResultCode::kActorUiError))
      .WillRepeatedly(UiEventResult<ToolRequest>(mojom::ActionResultCode::kOk));
  ActorTask& task =
      CreateTaskWithUiEventDispatcher(std::move(ui_event_dispatcher));

  ToolRequestPtr composite = MakeFakeCompositeToolRequest({
      StepLoggingToolRequest(execution_log, "failing_tool"),
      StepLoggingToolRequest(execution_log, "fallback_tool"),
      ReturnResult(ToolResult::Success()),
  });

  ActResultFuture result;
  task.Act(ToRequestList(composite), result.GetCallback());
  ExpectOkResult(result);

  // The first tool fails before being invoked so it never logs.
  EXPECT_THAT(execution_log, testing::ElementsAre("fallback_tool"));
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_name, "FakeCompositeTool");
}

// Verifies that a nested dispatcher can recover from a sub-tool that fails in
// the UI post-invoke stage (i.e. after the tool was invoked successfully) by
// issuing a fallback sub-tool that succeeds.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       NestedDispatcherHandlesUiPostInvokeFailureWithFallback) {
  std::vector<std::string> execution_log;

  auto ui_event_dispatcher = MakeSucceedingMockUiEventDispatcher();
  EXPECT_CALL(*ui_event_dispatcher, OnPostTool)
      .WillOnce(
          UiEventResult<ToolRequest>(mojom::ActionResultCode::kActorUiError))
      .WillRepeatedly(UiEventResult<ToolRequest>(mojom::ActionResultCode::kOk));
  ActorTask& task =
      CreateTaskWithUiEventDispatcher(std::move(ui_event_dispatcher));

  ToolRequestPtr composite = MakeFakeCompositeToolRequest({
      StepLoggingToolRequest(execution_log, "failing_tool"),
      StepLoggingToolRequest(execution_log, "fallback_tool"),
      ReturnResult(ToolResult::Success()),
  });

  ActResultFuture result;
  task.Act(ToRequestList(composite), result.GetCallback());
  ExpectOkResult(result);

  // The first tool is invoked before the UI post-invoke stage fails.
  EXPECT_THAT(execution_log,
              testing::ElementsAre("failing_tool", "fallback_tool"));
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].tool_name, "FakeCompositeTool");
}

// Verifies that pausing a task while a nested sub-tool is in flight cancels
// execution and unwinds the dispatcher stack without calling GetNext again.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       PauseCancelsNestedDispatcherWithoutCallingGetNext) {
  std::vector<std::string> execution_log;
  base::test::TestFuture<DeprecatedToolCallback> sub_tool_invoked;

  ToolRequestPtr first =
      std::make_unique<LoggingToolRequest>(execution_log, "first");
  ToolRequestPtr composite = MakeFakeCompositeToolRequest({
      StepInvokeTestFuture(sub_tool_invoked),
      ExpectStepNotCalled(),
  });

  ActResultFuture result;
  actor_task().Act(ToRequestList(first, composite), result.GetCallback());

  ASSERT_TRUE(sub_tool_invoked.Wait());
  EXPECT_THAT(execution_log, testing::ElementsAre("first"));

  actor_task().Pause(/*from_actor=*/false);
  ExpectErrorResult(result, mojom::ActionResultCode::kTaskPaused);

  // Flush ExecutionEngine's posted cancellation callback before starting a new
  // Act().
  WaitForPostedTask();

  actor_task().Resume();

  ToolRequestPtr after_resume =
      std::make_unique<LoggingToolRequest>(execution_log, "after_resume");
  ActResultFuture resumed_result;
  actor_task().Act(ToRequestList(after_resume), resumed_result.GetCallback());
  ExpectOkResult(resumed_result);
  EXPECT_THAT(execution_log, testing::ElementsAre("first", "after_resume"));
}

// Verifies that screenshot and page content extraction policy votes from
// nested sub-tools are aggregated into the Act() TabObservationStrategy.
IN_PROC_BROWSER_TEST_F(ExecutionEngineDispatcherBrowserTest,
                       CollectsToolVotesFromNestedSubTools) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::vector<std::string> execution_log;
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  mojom::ActionResultPtr r1 =
      MakeOkResult(/*requires_page_stabilization=*/false);
  r1->screenshot_policy = mojom::ScreenshotPolicy::kSkipped;
  r1->page_content_policy = mojom::PageContentExtractionPolicy::kRequired;

  mojom::ActionResultPtr r2 =
      MakeOkResult(/*requires_page_stabilization=*/false);
  r2->screenshot_policy = mojom::ScreenshotPolicy::kRequired;
  r2->page_content_policy = mojom::PageContentExtractionPolicy::kSkipped;

  ToolRequestPtr composite = MakeFakeCompositeToolRequest(
      {
          StepLoggingToolRequest(execution_log, "vote_1", std::move(r1),
                                 surface),
          StepLoggingToolRequest(execution_log, "vote_2", std::move(r2),
                                 surface),
          ReturnResult(ToolResult::Success()),
      },
      "FakeCompositeTool", surface);

  ActResultFuture result;
  actor_task().Act(ToRequestList(composite), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_THAT(execution_log, testing::ElementsAre("vote_1", "vote_2"));
  const TabObservationStrategy& strategy = result.GetStrategy();
  EXPECT_EQ(strategy.GetScreenshotPolicy(surface), ScreenshotPolicy::kRequired);
  EXPECT_EQ(strategy.GetPageContentExtractionPolicy(surface),
            PageContentExtractionPolicy::kRequired);
}

class ExecutionEngineDispatcherImplicitOriginGrantBrowserTest
    : public ExecutionEngineDispatcherBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ExecutionEngineDispatcherImplicitOriginGrantBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kGlicCrossOriginNavigationGating,
        {{kGlicConfirmNavigationToNewOrigins.name, "true"},
         {kGlicAllowImplicitToolOriginGrants.name,
          allow_implicit_tool_origin_grants() ? "true" : "false"}});
  }

  bool allow_implicit_tool_origin_grants() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that implicit origin grants associated with sub-tools returned by a
// nested dispatcher are applied when executing each sub-tool.
IN_PROC_BROWSER_TEST_P(ExecutionEngineDispatcherImplicitOriginGrantBrowserTest,
                       ImplicitOriginGrantAppliedToSubTool) {
  const GURL start_url =
      embedded_https_test_server().GetURL("a.com", "/actor/blank.html");
  const GURL cross_origin_url =
      embedded_https_test_server().GetURL("b.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  ToolRequestPtr navigate_tool =
      MakeNavigateRequest(*active_tab(), cross_origin_url.spec());
  ASSERT_EQ(navigate_tool->AssociatedOriginGrant(),
            url::Origin::Create(cross_origin_url));

  ToolRequestPtr composite = MakeFakeCompositeToolRequest(
      {
          Step(std::move(navigate_tool)),
          base::BindLambdaForTesting(
              [](std::optional<ToolResultWithLatencyInfo> prev_result)
                  -> ExecutionEngineDispatcher::NextAction {
                if (!prev_result->tool_result.Ok()) {
                  return MakeCompositeResult(ToolResult::Error(
                      ToolError(prev_result->tool_result.error()->code)));
                }
                return MakeCompositeResult(ToolResult::Success());
              }),
      },
      "FakeCompositeTool", ActorSurfaceHandle::From(active_tab()->GetHandle()));
  ASSERT_EQ(composite->AssociatedOriginGrant(), std::nullopt);

  ActResultFuture result;
  actor_task().Act(ToRequestList(composite), result.GetCallback());
  if (!allow_implicit_tool_origin_grants()) {
    ExpectErrorResult(result,
                      mojom::ActionResultCode::kTriggeredNavigationBlocked);
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), start_url);
    return;
  }

  ExpectOkResult(result);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), cross_origin_url);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExecutionEngineDispatcherImplicitOriginGrantBrowserTest,
    testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "ImplicitGrantsEnabled" : "ImplicitGrantsDisabled";
    });

}  // namespace

}  // namespace actor

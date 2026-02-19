// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TEST_UTIL_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TEST_UTIL_H_

#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <vector>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/enterprise_policy_url_checker.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/task_id.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/protobuf/src/google/protobuf/descriptor.h"
#include "ui/gfx/geometry/point.h"

namespace base {
class CommandLine;
}  // namespace base

namespace content {
class RenderFrameHost;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace actor {
template <typename T>
auto UiEventDispatcherCallback(
    base::RepeatingCallback<mojom::ActionResultPtr()> result_fn) {
  return [result_fn = std::move(result_fn)](
             const T&,
             ui::UiEventDispatcher::UiCompleteCallback callback) mutable {
    std::move(callback).Run(result_fn.Run());
  };
}

using ActResultFuture =
    base::test::TestFuture<mojom::ActionResultPtr,
                           std::optional<size_t>,
                           std::vector<ActionResultWithLatencyInfo>>;
using PerformActionsFuture =
    base::test::TestFuture<mojom::ActionResultCode,
                           std::optional<size_t>,
                           std::vector<ActionResultWithLatencyInfo>>;

/////////////////////////
// Proto action makers

optimization_guide::proto::Actions MakeClick(
    content::RenderFrameHost& rfh,
    int content_node_id,
    optimization_guide::proto::ClickAction::ClickType click_type,
    optimization_guide::proto::ClickAction::ClickCount click_count,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeClick(
    tabs::TabHandle tab_handle,
    const gfx::Point& click_point,
    optimization_guide::proto::ClickAction::ClickType click_type,
    optimization_guide::proto::ClickAction::ClickCount click_count,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeHistoryBack(
    tabs::TabHandle tab_handle,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeHistoryForward(
    tabs::TabHandle tab_handle,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeMouseMove(
    content::RenderFrameHost& rfh,
    int content_node_id,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeMouseMove(
    tabs::TabHandle tab_handle,
    const gfx::Point& move_point,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeNavigate(
    tabs::TabHandle tab_handle,
    std::string_view target_url,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeCreateTab(
    SessionID window_id,
    bool foreground,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeActivateWindow(
    SessionID window_id,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeCreateWindow(
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeCloseWindow(
    SessionID window_id,
    std::optional<actor::TaskId> task_id = std::nullopt);

optimization_guide::proto::Actions MakeType(
    content::RenderFrameHost& rfh,
    int content_node_id,
    std::string_view text,
    bool follow_by_enter,
    optimization_guide::proto::TypeAction::TypeMode mode =
        optimization_guide::proto::TypeAction_TypeMode_DELETE_EXISTING,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeType(
    tabs::TabHandle tab_handle,
    const gfx::Point& type_point,
    std::string_view text,
    bool follow_by_enter,
    optimization_guide::proto::TypeAction::TypeMode mode =
        optimization_guide::proto::TypeAction_TypeMode_DELETE_EXISTING,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeSelect(
    content::RenderFrameHost& rfh,
    int content_node_id,
    std::string_view value,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeScroll(
    content::RenderFrameHost& rfh,
    std::optional<int> content_node_id,
    float scroll_offset_x,
    float scroll_offset_y,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeScroll(
    content::RenderFrameHost& rfh,
    const gfx::Point& scroll_point,
    float scroll_offset_x,
    float scroll_offset_y,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeScrollTo(
    content::RenderFrameHost& rfh,
    int content_node_id,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeDragAndRelease(
    tabs::TabHandle tab_handle,
    const gfx::Point& from_point,
    const gfx::Point& to_point,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeDragAndRelease(
    content::RenderFrameHost& rfh,
    int from_node_id,
    int to_node_id,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeWait(
    std::optional<base::TimeDelta> duration = std::nullopt,
    std::optional<tabs::TabHandle> observe_tab_handle = std::nullopt,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeScriptTool(
    content::RenderFrameHost& rfh,
    const std::string& name,
    const std::string& input_arguments,
    std::optional<actor::TaskId> task_id = std::nullopt);
optimization_guide::proto::Actions MakeMediaControl(
    tabs::TabHandle tab_handle,
    MediaControl media_control,
    std::optional<actor::TaskId> task_id = std::nullopt);

/////////////////////////
// ToolRequest action makers

std::unique_ptr<ToolRequest> MakeClickRequest(content::RenderFrameHost& rfh,
                                              int content_node_id);
std::unique_ptr<ToolRequest> MakeClickRequest(tabs::TabInterface& tab,
                                              const gfx::Point& click_point);
std::unique_ptr<ToolRequest> MakeHistoryBackRequest(tabs::TabInterface& tab);
std::unique_ptr<ToolRequest> MakeHistoryForwardRequest(tabs::TabInterface& tab);
std::unique_ptr<ToolRequest> MakeMouseMoveRequest(content::RenderFrameHost& rfh,
                                                  int content_node_id);
std::unique_ptr<ToolRequest> MakeMouseMoveRequest(tabs::TabInterface& tab,
                                                  const gfx::Point& move_point);
std::unique_ptr<ToolRequest> MakeNavigateRequest(tabs::TabInterface& tab,
                                                 std::string_view target_url);
std::unique_ptr<ToolRequest> MakeTypeRequest(content::RenderFrameHost& rfh,
                                             int content_node_id,
                                             std::string_view text,
                                             bool follow_by_enter);
std::unique_ptr<ToolRequest> MakeTypeRequest(tabs::TabInterface& tab,
                                             const gfx::Point& type_point,
                                             std::string_view text,
                                             bool follow_by_enter);
std::unique_ptr<ToolRequest> MakeSelectRequest(content::RenderFrameHost& rfh,
                                               int content_node_id,
                                               std::string_view value);
std::unique_ptr<ToolRequest> MakeScrollRequest(
    content::RenderFrameHost& rfh,
    std::optional<int> content_node_id,
    float scroll_offset_x,
    float scroll_offset_y);
std::unique_ptr<ToolRequest> MakeScrollToRequest(content::RenderFrameHost& rfh,
                                                 int content_node_id);
std::unique_ptr<ToolRequest> MakeDragAndReleaseRequest(
    tabs::TabInterface& tab,
    const gfx::Point& from_point,
    const gfx::Point& to_point);
std::unique_ptr<ToolRequest> MakeWaitRequest(
    tabs::TabInterface* observe_tab = nullptr);
std::unique_ptr<ToolRequest> MakeCreateTabRequest(SessionID window_id,
                                                  bool foreground);
std::unique_ptr<ToolRequest> MakeActivateTabRequest(tabs::TabHandle tab);
std::unique_ptr<ToolRequest> MakeCloseTabRequest(tabs::TabHandle tab);
std::unique_ptr<ToolRequest> MakeAttemptLoginRequest(
    tabs::TabInterface& tab,
    std::optional<PageTarget> password_button = std::nullopt,
    std::optional<PageTarget> sign_in_with_google_button = std::nullopt);
std::unique_ptr<ToolRequest> MakeScriptToolRequest(
    content::RenderFrameHost& rfh,
    const std::string& name,
    const std::string& input_arguments);
std::unique_ptr<ToolRequest> MakeMediaControlRequest(
    tabs::TabInterface& tab,
    MediaControl media_control);

// A helper to create a vector of ToolRequests suitable for passing to
// ExecutionEngine::Act. Note that this will necessarily move the ToolRequest
// from the unique_ptr parameters into the new list.
template <typename T, typename... Args>
std::vector<std::unique_ptr<ToolRequest>> ToRequestList(T&& first,
                                                        Args&&... rest) {
  using DecayedT = std::decay_t<T>;

  static_assert(std::is_same_v<DecayedT, std::unique_ptr<ToolRequest>>,
                "All arguments must be unique_ptr<ToolRequest>.");
  static_assert((std::is_same_v<DecayedT, std::decay_t<Args>> && ...),
                "All arguments must be unique_ptr<ToolRequest>.");

  std::vector<std::unique_ptr<ToolRequest>> items;
  items.reserve(1 + sizeof...(rest));
  items.push_back(std::move(first));
  (items.push_back(std::move(rest)), ...);

  return items;
}

void ExpectOkResult(const mojom::ActionResult& result);
void ExpectOkResult(base::test::TestFuture<mojom::ActionResultPtr>& future);
void ExpectOkResult(ActResultFuture& future);
void ExpectErrorResult(ActResultFuture& future,
                       mojom::ActionResultCode expected_code);
void ExpectOkResult(PerformActionsFuture& future);
void ExpectErrorResult(PerformActionsFuture& future,
                       mojom::ActionResultCode expected_code);

// Sets up GLIC_ACTION_PAGE_BLOCK to block the given host via component updater.
bool SetUpOptimizationGuideComponentBlocklist(const base::FilePath& path,
                                              const std::string& blocked_host);

// Sets up GLIC_ACTION_PAGE_BLOCK to block the given host via the command line.
void SetUpBlocklist(base::CommandLine* command_line,
                    const std::string& blocked_host);

// Waits until a posted task is invoked. Used to ensures any prior posted tasks
// are run (assuming a sequenced task runner).
void WaitForPostedTask();

// Helper to parse a Base64 string into a protobuf of type `ProtoType`.
template <typename ProtoType>
base::expected<ProtoType, std::string> ParseBase64Proto(
    std::string_view base64_string) {
  std::string decoded_result;
  if (!base::Base64Decode(base64_string, &decoded_result)) {
    return base::unexpected(
        base::StrCat({"Failed to Base64-decode the result (", base64_string,
                      ") from JavaScript."}));
  }
  ProtoType proto_result;
  proto_result.ParseFromString(decoded_result);
  return base::ok(proto_result);
}

// Helper used to wait on an ExecutionEngine state transition. The provided
// callback is synchronously invoked when ExecutionEngine transitions to the
// target state.
class ExecutionEngineStateWaiter : public ExecutionEngine::StateObserver {
 public:
  ExecutionEngineStateWaiter(base::OnceClosure callback,
                             ExecutionEngine& execution_engine,
                             ExecutionEngine::State target_state);
  ~ExecutionEngineStateWaiter() override;

  // `ExecutionEngine::StateObserver`:
  void OnStateChanged(ExecutionEngine::State old_state,
                      ExecutionEngine::State new_state) override;

 private:
  base::OnceClosure callback_;
  const base::WeakPtr<ExecutionEngine> execution_engine_;
  ExecutionEngine::State target_state_;
};

class ActorTaskStateWaiter {
 public:
  ActorTaskStateWaiter(base::OnceClosure callback,
                       ActorKeyedService& service,
                       ActorTask& task,
                       ActorTask::State target_state);
  ~ActorTaskStateWaiter();

 private:
  void StateChanged(TaskId task_id, ActorTask::State state);

  base::OnceClosure callback_;
  TaskId task_id_;
  ActorTask::State target_state_;
  base::CallbackListSubscription subscription_;
};

// Use this RAII helper to provide a factory function for constructing
// ExecutionEngine. This allows tests to provide a mock ExecutionEngine or one
// constructed specially to be instrumented for testing.
class ScopedExecutionEngineFactory {
 public:
  explicit ScopedExecutionEngineFactory(
      ExecutionEngine::FactoryFunction factory);
  ~ScopedExecutionEngineFactory();
};

class MockPolicyChecker : public EnterprisePolicyUrlChecker {
 public:
  explicit MockPolicyChecker(EnterprisePolicyBlockReason reason);
  ~MockPolicyChecker();

  EnterprisePolicyBlockReason Evaluate(const GURL& url) const override;
 private:
  EnterprisePolicyBlockReason reason_;
};

// Returns a passthrough EnterprisePolicyUrlChecker tests can use to avoid
// policy checks.
const EnterprisePolicyUrlChecker* NoEnterprisePolicyChecker();

// Helper struct for unit tests that require a mock TabInterface and its
// associated ActorTabData.
struct TestTabState {
  explicit TestTabState(content::WebContents* web_contents = nullptr);
  ~TestTabState();

  using WillDetachCallbackList =
      base::RepeatingCallbackList<void(tabs::TabInterface*,
                                       tabs::TabInterface::DetachReason)>;
  WillDetachCallbackList will_detach_callback_list_;

  tabs::MockTabInterface tab;
  ::ui::UnownedUserDataHost user_data_host;
  std::unique_ptr<ActorTabData> tab_data;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TEST_UTIL_H_

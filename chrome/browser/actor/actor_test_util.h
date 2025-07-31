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

#include "base/metrics/field_trial_params.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
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

/////////////////////////
// Proto action makers

optimization_guide::proto::Actions MakeClick(content::RenderFrameHost& rfh,
                                             int content_node_id);
optimization_guide::proto::Actions MakeClick(tabs::TabHandle tab_handle,
                                             const gfx::Point& click_point);
optimization_guide::proto::Actions MakeHistoryBack(tabs::TabHandle tab_handle);
optimization_guide::proto::Actions MakeHistoryForward(
    tabs::TabHandle tab_handle);
optimization_guide::proto::Actions MakeMouseMove(content::RenderFrameHost& rfh,
                                                 int content_node_id);
optimization_guide::proto::Actions MakeMouseMove(const gfx::Point& move_point);
optimization_guide::proto::Actions MakeNavigate(tabs::TabHandle tab_handle,
                                                std::string_view target_url);
optimization_guide::proto::Actions MakeCreateTab(SessionID window_id,
                                                 bool foreground);
optimization_guide::proto::Actions MakeType(content::RenderFrameHost& rfh,
                                            int content_node_id,
                                            std::string_view text,
                                            bool follow_by_enter);
optimization_guide::proto::Actions MakeType(const gfx::Point& type_point,
                                            std::string_view text,
                                            bool follow_by_enter);
optimization_guide::proto::Actions MakeSelect(content::RenderFrameHost& rfh,
                                              int content_node_id,
                                              std::string_view value);
optimization_guide::proto::Actions MakeScroll(
    content::RenderFrameHost& rfh,
    std::optional<int> content_node_id,
    float scroll_offset_x,
    float scroll_offset_y);
optimization_guide::proto::Actions MakeDragAndRelease(
    const gfx::Point& from_point,
    const gfx::Point& to_point);
optimization_guide::proto::Actions MakeWait();
optimization_guide::proto::Actions MakeAttemptLogin();
optimization_guide::proto::Actions MakeScriptTool(
    content::RenderFrameHost& rfh,
    const std::string& name,
    const std::string& input_arguments);

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
std::unique_ptr<ToolRequest> MakeDragAndReleaseRequest(
    tabs::TabInterface& tab,
    const gfx::Point& from_point,
    const gfx::Point& to_point);
std::unique_ptr<ToolRequest> MakeWaitRequest();
std::unique_ptr<ToolRequest> MakeCreateTabRequest(SessionID window_id,
                                                  bool foreground);
std::unique_ptr<ToolRequest> MakeAttemptLoginRequest(tabs::TabInterface& tab);
std::unique_ptr<ToolRequest> MakeScriptToolRequest(
    content::RenderFrameHost& rfh,
    const std::string& name,
    const std::string& input_arguments);

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

  // This is a hack to push_back each item from the pack using pack expansion.
  // Fold expressions would make this cleaner but aren't yet allowed in
  // Chromium.
  int dummy[] = {0, (items.push_back(std::move(rest)), 0)...};
  (void)dummy;

  return items;
}

void ExpectOkResult(const mojom::ActionResult& result);
void ExpectOkResult(base::test::TestFuture<mojom::ActionResultPtr>& future);
void ExpectOkResult(base::test::TestFuture<mojom::ActionResultPtr,
                                           std::optional<size_t>>& future);
void ExpectErrorResult(base::test::TestFuture<mojom::ActionResultPtr,
                                              std::optional<size_t>>& future,
                       mojom::ActionResultCode expected_code);

// Sets up GLIC_ACTION_PAGE_BLOCK to block the given host.
void SetUpBlocklist(base::CommandLine* command_line,
                    const std::string& blocked_host);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TEST_UTIL_H_

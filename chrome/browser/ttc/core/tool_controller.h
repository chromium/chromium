// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TOOL_CONTROLLER_H_
#define CHROME_BROWSER_TTC_CORE_TOOL_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "components/tabs/public/tab_interface.h"
#include "components/ttc/app/public/tool_types.h"

class Profile;

namespace actor {
class ToolRequest;
}  // namespace actor

namespace content {
class Page;
}  // namespace content

namespace page_content_annotations {
class PageContextFetcher;
struct FetchPageContextResult;
struct FetchPageContextErrorDetails;
}  // namespace page_content_annotations

namespace ttc {

class SessionControllerImpl;

// Executes the session's tool calls in an actor task.
class ToolController {
 public:
  explicit ToolController(SessionControllerImpl& session_controller);
  ~ToolController();

  void ProcessToolCall(const ToolRequest& tool_request,
                       ToolResponseCallback callback);

  // Queues the calls in `tool_calls` to run in order, like consecutive
  // `ProcessToolCall()` calls. If any tool call is still running or queued, the
  // whole batch is rejected instead: each call is answered with a
  // `kExecutorBusy` error without being run. `tool_calls` must not be empty.
  void ProcessToolCallBatch(std::vector<PendingToolCall> tool_calls);

  // Returns the definitions of the tools this controller can execute, for
  // registration with the model backend.
  std::vector<ToolDefinition> GetToolDefinitions();

 private:
  Profile* GetProfile();

  // Whether no tool call is running or queued.
  bool IsIdle() const;

  // Adds `tool_call` to the back of `pending_tool_calls_`, without running it.
  void EnqueueToolCall(PendingToolCall tool_call);

  // Moves the tool call at the front of `pending_tool_calls_`, which must not
  // be empty, into `in_flight_tool_call_` and runs it. No call may be in
  // flight.
  void RunNextToolCall();

  // Ensures the session has an actor task and dispatches `tool_request` to the
  // tool implementation.
  void RunToolCall(const ToolRequest& tool_request,
                   ToolResponseCallback callback);

  // Called when the in-flight tool call finishes. Clears
  // `in_flight_tool_call_`, schedules the next queued tool call, if any, and
  // replies to the finished call with `response`.
  void OnToolCallDone(ToolResponse response);

  // Tool implementations. Please keep sorted, with a tool's completion
  // callbacks directly after it.
  void ClickElement(const base::DictValue& arguments,
                    ToolResponseCallback callback);
  void CloseCurrentTab(ToolResponseCallback callback);
  void CloseCurrentWindow(ToolResponseCallback callback);
  void CloseGeminiPanel(ToolResponseCallback callback);
  void CreateNewTab(ToolResponseCallback callback);
  void CreateNewWindow(ToolResponseCallback callback);
  void FindAndHighlight(const base::DictValue& arguments,
                        ToolResponseCallback callback);
  void GetPointer(ToolResponseCallback callback);
  void GetScreenshot(ToolResponseCallback callback);
  void OnGetScreenshotUrlChecked(base::WeakPtr<content::Page> page,
                                 ToolResponseCallback callback,
                                 bool is_url_supported);
  void OnGetScreenshotFetched(
      ToolResponseCallback callback,
      base::expected<
          std::unique_ptr<page_content_annotations::FetchPageContextResult>,
          page_content_annotations::FetchPageContextErrorDetails> result);
  void GoBack(ToolResponseCallback callback);
  void GoForward(ToolResponseCallback callback);
  void OpenKnownPage(const base::DictValue& arguments,
                     ToolResponseCallback callback);
  void OpenUrl(const base::DictValue& arguments, ToolResponseCallback callback);
  void PauseVideo(ToolResponseCallback callback);
  void PerformSearch(const base::DictValue& arguments,
                     ToolResponseCallback callback);
  void PlayVideo(ToolResponseCallback callback);
  void PromptGeminiPanel(const base::DictValue& arguments,
                         ToolResponseCallback callback);
  void ReloadPage(ToolResponseCallback callback);
  void ScrollPage(const base::DictValue& arguments,
                  ToolResponseCallback callback);
  void SeekToTimestamp(const base::DictValue& arguments,
                       ToolResponseCallback callback);
  void SelectOption(const base::DictValue& arguments,
                    ToolResponseCallback callback);
  void SetFullscreen(bool fullscreen, ToolResponseCallback callback);
  void SetText(const base::DictValue& arguments, ToolResponseCallback callback);
  void SwitchTab(const base::DictValue& arguments,
                 ToolResponseCallback callback);
  void TranslatePage(const base::DictValue& arguments,
                     ToolResponseCallback callback);

  // Runs the tool request returned by `create_action` against the contents
  // tracked by the session (see
  // SessionControllerImpl::GetVoiceFocusedWebContents()), replying to
  // `callback` with the result. Replies with an error if there's no actor
  // surface to act on, in which case `create_action` isn't invoked.
  void PerformActionOnTrackedContents(
      base::FunctionRef<std::unique_ptr<actor::ToolRequest>(
          actor::ActorSurfaceHandle)> create_action,
      ToolResponseCallback callback);

  // Runs `action` in the session's actor task and replies to `callback` with
  // the result.
  void PerformAction(std::unique_ptr<actor::ToolRequest> action,
                     ToolResponseCallback callback);

  void OnActionsFinished(
      ToolResponseCallback callback,
      std::vector<actor::ActionResultWithLatencyInfo> results,
      actor::TabObservationStrategy strategy);

  // Owns this object.
  const raw_ref<SessionControllerImpl> session_controller_;

  // The tool call that's currently running, if any.
  std::optional<PendingToolCall> in_flight_tool_call_;

  // Tool calls waiting to run, in the order they were received. If this is
  // non-empty and no call is in flight, the front call is about to run from a
  // posted task.
  base::circular_deque<PendingToolCall> pending_tool_calls_;

  std::unique_ptr<page_content_annotations::PageContextFetcher>
      screenshot_fetcher_;

  base::WeakPtrFactory<ToolController> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TOOL_CONTROLLER_H_

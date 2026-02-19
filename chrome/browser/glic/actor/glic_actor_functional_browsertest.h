// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_FUNCTIONAL_BROWSERTEST_H_
#define CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_FUNCTIONAL_BROWSERTEST_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/test_support/glic_functional_browsertest.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/common/actor_webui.mojom.h"

namespace glic::actor {

using ::actor::ActorTask;
using ::actor::TaskId;
using ::base::test::TestFuture;
using ::optimization_guide::proto::Actions;
using ::optimization_guide::proto::ActionsResult;
using ::optimization_guide::proto::TabObservation;
using ::page_content_annotations::FetchPageContextResult;

MATCHER_P(HasResultCode, expected_code, "") {
  return arg.action_result() == static_cast<int32_t>(expected_code);
}

// Helper to mock the result returned on a TabObservation built using
// actor::BuildActionsResultWithObservations. While live, use the provided
// function to set TabObservationResults. Unset on destruction.
class ScopedMockTabObservationResult {
 public:
  explicit ScopedMockTabObservationResult(
      base::RepeatingCallback<void(TabObservation*,
                                   const FetchPageContextResult&)> callback);
  ~ScopedMockTabObservationResult();
};

// Helper class that utilizes content::DOMMessageQueue to capture the result of
// an asynchronous PerformActions call. It listens for messages sent via
// domAutomationController and filters by request ID to ensure the correct
// result is returned.
class AsyncActionWaiter {
 public:
  AsyncActionWaiter(content::RenderFrameHost* rfh, std::string request_id);

  base::expected<ActionsResult, std::string> Wait();

 private:
  content::DOMMessageQueue queue_;
  std::string request_id_;
};

class GlicActorFunctionalBrowserTestBase
    : public glic::test::GlicFunctionalBrowserTestBase {
 public:
  static constexpr base::TimeDelta kShortWaitTime = base::Milliseconds(10);
  static constexpr base::TimeDelta kLongWaitTime = base::Minutes(2);

  GlicActorFunctionalBrowserTestBase();
  ~GlicActorFunctionalBrowserTestBase() override;

 protected:
  void SetUpOnMainThread() override;

  ::actor::ActorKeyedService* actor_keyed_service();

  // Helper that sets a future if an ActorTask with `task_id` enters a completed
  // state.
  base::CallbackListSubscription CreateTaskCompletionSubscription(
      TaskId for_task_id,
      TestFuture<ActorTask::State>& future);

  // Returns the state of the relevant ActorTask.
  ActorTask::State GetActorTaskState(TaskId task_id);

  base::expected<glic::mojom::CancelActionsResult, std::string> CancelActions(
      TaskId task_id);

  // Helper to call the CreateTask TS API.
  // Returns the TaskId of the newly created ActorTask.
  base::expected<TaskId, std::string> CreateTask(
      ::actor::webui::mojom::TaskOptionsPtr options = nullptr);

  // Helper to call the PerformActions TS API synchronously.
  // Takes an `Actions` proto and returns the resulting `ActionsResult` proto.
  // Note: This blocks until all Actions are completed by wrapping
  // PerformActionsAsync.
  [[nodiscard]] base::expected<ActionsResult, std::string> PerformActions(
      const Actions& actions);

  // Helper to run PerformActions asynchronously.
  // Returns an AsyncActionWaiter that can be used to wait for the result.
  [[nodiscard]] std::unique_ptr<AsyncActionWaiter> PerformActionsAsync(
      const Actions& actions);

  // Helper to call the StopActorTask TS API.
  // Note: Inactive tasks are cleared right after entering a "Completed" state,
  // so you need to listen for state changes using a subscription before calling
  // this method if you want to verify the task stopped correctly.
  void StopActorTask(TaskId task_id,
                     glic::mojom::ActorTaskStopReason stop_reason);

  // Helper to call the PauseActorTask TS API.
  // Note: `tab_handle` needs to be specified if you intend to resume the task
  // in the future without performing any tab-scoped actions beforehand.
  void PauseActorTask(TaskId task_id,
                      glic::mojom::ActorTaskPauseReason pause_reason =
                          glic::mojom::ActorTaskPauseReason::kPausedByModel,
                      tabs::TabHandle tab_handle = tabs::TabHandle::Null());

  // Helper to call the ResumeActorTask TS API.
  // Returns the ActionResultCode of the resumeActorTask call.
  base::expected<::actor::mojom::ActionResultCode, std::string> ResumeActorTask(
      TaskId task_id,
      base::Value context_options);

  // Helper to call the InterruptActorTask TS API.
  void InterruptActorTask(TaskId task_id);

  // Helper to call the UninterruptActorTask TS API.
  void UninterruptActorTask(TaskId task_id);

  // Waits until the task reaches the `expected_state`.
  void WaitForTaskState(TaskId task_id, ActorTask::State expected_state);

  // Helper to call the CreateActorTab TS API.
  // Returns the TabId of the newly created tab, or base::unexpected on failure.
  base::expected<tabs::TabHandle, std::string> CreateActorTab(
      TaskId task_id,
      std::optional<bool> open_in_background,
      std::optional<std::string> initiator_tab_id,
      std::optional<std::string> initiator_window_id);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace glic::actor

#endif  // CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_FUNCTIONAL_BROWSERTEST_H_

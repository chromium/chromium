// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_functional_browsertest.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/test/browser_test.h"

namespace mojo {

base::Value ConvertToValue(const page_content_annotations::ScreenshotOptions::
                               ScreenshotCollectionOptions& in) {
  base::Value raw_out(base::Value::Type::DICT);
  base::DictValue& out = raw_out.GetDict();
  out.Set("maxWidth", static_cast<int>(in.max_width.value_or(0)));
  out.Set("maxHeight", static_cast<int>(in.max_height.value_or(0)));
  out.Set("screenshotImageFormat",
          static_cast<int>(in.screenshot_image_format.value_or(
              page_content_annotations::ScreenshotOptions::
                  ScreenshotImageFormat::kJpeg)));
  out.Set("screenshotCompressionQuality",
          static_cast<int>(in.screenshot_compression_quality.value_or(
              page_content_annotations::ScreenshotOptions::
                  ScreenshotCompressionQuality::kMedium)));
  return raw_out;
}

template <>
struct TypeConverter<base::Value, glic::mojom::GetTabContextOptions> {
  static base::Value Convert(const glic::mojom::GetTabContextOptions& in) {
    base::Value raw_out(base::Value::Type::DICT);
    base::DictValue& out = raw_out.GetDict();
    out.Set("includeInnerText", in.include_inner_text);
    out.Set("innerTextBytesLimit", static_cast<int>(in.inner_text_bytes_limit));
    out.Set("includeViewportScreenshot", in.include_viewport_screenshot);
    out.Set("includeAnnotatedPageContent", in.include_annotated_page_content);
    out.Set("maxMetaTags", static_cast<int>(in.max_meta_tags));
    out.Set("includePdf", in.include_pdf);
    out.Set("pdfSizeLimit", static_cast<int>(in.pdf_size_limit));
    out.Set("annotatedPageContentMode",
            static_cast<int>(in.annotated_page_content_mode));
    out.Set("screenshotCollectionOptions",
            ConvertToValue(in.screenshot_collection_options));
    return raw_out;
  }
};
}  // namespace mojo

namespace glic::actor {
namespace {

using ::base::test::TestFuture;
using ::base::test::ValueIs;
using ::glic::test::ErrorHasSubstr;
using ::optimization_guide::proto::Actions;

// Helper class to observe journal entries and wait for a specific condition.
class JournalObserver : public ::actor::AggregatedJournal::Observer {
 public:
  using Predicate =
      base::RepeatingCallback<bool(const ::actor::mojom::JournalEntry&)>;

  explicit JournalObserver(::actor::AggregatedJournal* journal)
      : journal_(journal) {
    journal_->AddObserver(this);
  }

  ~JournalObserver() override { journal_->RemoveObserver(this); }

  void WillAddJournalEntry(
      const ::actor::AggregatedJournal::Entry& entry) override {
    if (wait_predicate_ && wait_predicate_.Run(*entry.data)) {
      if (run_loop_) {
        run_loop_->Quit();
      }
    }
  }

  // Waits until a journal entry matching the predicate is observed.
  // NOTE: Only entries added after this method is called will be considered.
  void WaitUntil(Predicate predicate) {
    wait_predicate_ = std::move(predicate);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  raw_ptr<::actor::AggregatedJournal> journal_;
  Predicate wait_predicate_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

bool JournalEntryHasError(const ::actor::mojom::JournalEntry& entry,
                          const std::string& error_message) {
  for (const auto& detail : entry.details) {
    if (detail->key == "error" && detail->value == error_message) {
      return true;
    }
  }
  return false;
}

class GlicActorTaskLifecycleFunctionalBrowserTest
    : public GlicActorFunctionalBrowserTestBase {
 public:
  GlicActorTaskLifecycleFunctionalBrowserTest() = default;
  ~GlicActorTaskLifecycleFunctionalBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       PauseAndResumeCreatedTask) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription completion_subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  PauseActorTask(task_id, glic::mojom::ActorTaskPauseReason::kPausedByUser,
                 active_tab()->GetHandle());
  // Wait for the task to pause.
  WaitForTaskState(task_id, ActorTask::State::kPausedByUser);

  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  Actions action = ::actor::MakeNavigate(active_tab()->GetHandle(),
                                         target_url.spec(), task_id);

  // Performing an action on a paused task should fail.
  EXPECT_THAT(
      PerformActions(action),
      ValueIs(HasResultCode(::actor::mojom::ActionResultCode::kTaskPaused)));
  EXPECT_NE(target_url, web_contents()->GetURL());

  EXPECT_THAT(
      ResumeActorTask(task_id,
                      glic::mojom::GetTabContextOptions().To<base::Value>()),
      ValueIs(::actor::mojom::ActionResultCode::kOk));
  EXPECT_EQ(ActorTask::State::kReflecting, GetActorTaskState(task_id));

  // Performing the action again should succeed.
  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(::actor::mojom::ActionResultCode::kOk)));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       PauseAndResumeInvalidTask) {
  TaskId invalid_task_id = TaskId(12345);
  ASSERT_EQ(actor_keyed_service()->GetTask(invalid_task_id), nullptr);

  JournalObserver observer(&actor_keyed_service()->GetJournal());
  // Pausing an invalid task should be a no-op and log an error.
  PauseActorTask(invalid_task_id,
                 glic::mojom::ActorTaskPauseReason::kPausedByUser,
                 active_tab()->GetHandle());
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to pause task" &&
               JournalEntryHasError(entry, "No such task");
      }));

  EXPECT_THAT(
      ResumeActorTask(invalid_task_id,
                      glic::mojom::GetTabContextOptions().To<base::Value>()),
      ErrorHasSubstr("resumeActorTask failed: No such task"));
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       PauseAndResumeInactiveTask) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription completion_subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";

  JournalObserver observer(&actor_keyed_service()->GetJournal());
  // Pausing an inactive task should be a no-op and log an error.
  PauseActorTask(task_id, glic::mojom::ActorTaskPauseReason::kPausedByUser,
                 active_tab()->GetHandle());
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to pause task" &&
               JournalEntryHasError(entry, "No such task");
      }));

  // Resuming a completed task should fail as it doesn't exist anymore.
  EXPECT_THAT(
      ResumeActorTask(task_id,
                      glic::mojom::GetTabContextOptions().To<base::Value>()),
      ErrorHasSubstr("resumeActorTask failed: No such task"));
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       PauseActiveTask) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  // Use a long wait to ensure we can pause before it completes.
  Actions wait_action =
      ::actor::MakeWait(kLongWaitTime, active_tab()->GetHandle(), task_id);

  std::unique_ptr<AsyncActionWaiter> action_waiter =
      PerformActionsAsync(wait_action);
  PauseActorTask(task_id, glic::mojom::ActorTaskPauseReason::kPausedByUser,
                 active_tab()->GetHandle());

  // Verify the WaitAction was ended and the task was paused.
  EXPECT_THAT(
      action_waiter->Wait(),
      ValueIs(HasResultCode(::actor::mojom::ActionResultCode::kTaskPaused)));
  WaitForTaskState(task_id, ActorTask::State::kPausedByUser);

  EXPECT_THAT(
      ResumeActorTask(task_id,
                      glic::mojom::GetTabContextOptions().To<base::Value>()),
      ValueIs(::actor::mojom::ActionResultCode::kOk));
  EXPECT_EQ(ActorTask::State::kReflecting, GetActorTaskState(task_id));

  // Verify new Actions can be performed after the task is resumed.
  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  Actions nav_action = ::actor::MakeNavigate(active_tab()->GetHandle(),
                                             target_url.spec(), task_id);

  EXPECT_THAT(PerformActions(nav_action),
              base::test::ValueIs(
                  HasResultCode(::actor::mojom::ActionResultCode::kOk)));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       StopActiveTaskWithModelError) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  Actions wait_action =
      ::actor::MakeWait(kLongWaitTime, active_tab()->GetHandle(), task_id);
  std::unique_ptr<AsyncActionWaiter> action_waiter =
      PerformActionsAsync(wait_action);

  // Wait for the task to start acting before stopping.
  WaitForTaskState(task_id, ActorTask::State::kActing);

  // Verify the action is ended with the appropriate code after task is stopped.
  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kModelError);
  EXPECT_THAT(
      action_waiter->Wait(),
      ValueIs(HasResultCode(::actor::mojom::ActionResultCode::kTaskWentAway)));

  EXPECT_EQ(ActorTask::State::kFailed, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFailed state.";
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       InterruptAndUninterruptInvalidTask) {
  JournalObserver observer(&actor_keyed_service()->GetJournal());
  TaskId invalid_task_id = TaskId(12345);
  ASSERT_EQ(actor_keyed_service()->GetTask(invalid_task_id), nullptr);

  // Interrupting an invalid task should be a no-op and log an error.
  InterruptActorTask(invalid_task_id);
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to interrupt task" &&
               JournalEntryHasError(entry, "No such task");
      }));

  // Uninterrupting an invalid task should be a no-op and log an error.
  UninterruptActorTask(invalid_task_id);
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to uninterrupt task" &&
               JournalEntryHasError(entry, "No such task");
      }));
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       InterruptAndUninterruptTaskWithCompletedActions) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  Actions action = ::actor::MakeNavigate(active_tab()->GetHandle(),
                                         target_url.spec(), task_id);

  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(::actor::mojom::ActionResultCode::kOk)));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  InterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kWaitingOnUser);

  // Ensure uninterrupting a task with no pending actions sets the state
  // to kReflecting
  UninterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kReflecting);

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       InterruptAndUninterruptActiveTaskAndPerformActions) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  // Use a long wait to ensure we can interrupt before it completes.
  Actions wait_action =
      ::actor::MakeWait(kLongWaitTime, active_tab()->GetHandle(), task_id);
  std::unique_ptr<AsyncActionWaiter> action_waiter =
      PerformActionsAsync(wait_action);

  // Wait for the task to start acting before interrupting.
  WaitForTaskState(task_id, ActorTask::State::kActing);

  InterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kWaitingOnUser);

  // Ensure uninterrupting a task with previously pending actions sets the state
  // to kActing
  UninterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kActing);

  // Since the ongoing long wait action must be completed before sending another
  // async action, we need to use the CancelActions API to cancel all the
  // ongoing actions on the task.
  EXPECT_THAT(CancelActions(task_id),
              base::test::ValueIs(glic::mojom::CancelActionsResult::kSuccess));
  EXPECT_THAT(action_waiter->Wait(),
              ValueIs(HasResultCode(
                  ::actor::mojom::ActionResultCode::kActionsCancelled)));

  // Ensure the task can still perform actions after being uninterrupted.
  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  Actions nav_action = ::actor::MakeNavigate(active_tab()->GetHandle(),
                                             target_url.spec(), task_id);
  EXPECT_THAT(PerformActions(nav_action),
              base::test::ValueIs(
                  HasResultCode(::actor::mojom::ActionResultCode::kOk)));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       InterruptWithReasons) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  // Use a long wait to ensure we can interrupt before it completes.
  Actions wait_action =
      ::actor::MakeWait(kLongWaitTime, active_tab()->GetHandle(), task_id);
  std::unique_ptr<AsyncActionWaiter> action_waiter =
      PerformActionsAsync(wait_action);

  // Wait for the task to start acting before interrupting.
  WaitForTaskState(task_id, ActorTask::State::kActing);

  // Test interrupting with NO reason.
  InterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kWaitingOnUser);
  UninterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kActing);

  // Test interrupting with a specific reason.
  InterruptActorTask(task_id,
                     mojom::ActorTaskInterruptReason::kWaitingUserInput);
  WaitForTaskState(task_id, ActorTask::State::kWaitingOnUser);
  UninterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kActing);

  // Test interrupting with another specific reason.
  InterruptActorTask(task_id,
                     mojom::ActorTaskInterruptReason::kWaitingUserConfirmation);
  WaitForTaskState(task_id, ActorTask::State::kWaitingOnUser);
  UninterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kActing);

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       ActuatingChangedCallback) {
  GlicInstanceImpl* instance = GetGlicInstanceImpl();
  ASSERT_TRUE(instance);
  GlicActorTaskManager* task_manager = instance->GetActorTaskManager();
  ASSERT_TRUE(task_manager);

  TestFuture<bool> actuating_true_future;
  TestFuture<bool> actuating_false_future;

  base::CallbackListSubscription subscription =
      task_manager->AddActuatingChangedCallback(
          base::BindLambdaForTesting([&](bool actuating) {
            if (actuating) {
              actuating_true_future.SetValue(true);
            } else {
              actuating_false_future.SetValue(false);
            }
          }));

  // Create a task and verify callback receives true.
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());
  EXPECT_TRUE(actuating_true_future.Get());

  // Stop the task and verify callback receives false.
  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_FALSE(actuating_false_future.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       ActivateTabWithConversationUsesActorState) {
  GlicInstanceImpl* instance = GetGlicInstanceImpl();
  ASSERT_TRUE(instance);

  // Register a conversation ID for the instance if not present.
  std::optional<std::string> conv_id_opt = instance->conversation_id();
  std::string conv_id = conv_id_opt.value_or("test_conversation_id");
  if (!conv_id_opt.has_value()) {
    auto info = mojom::ConversationInfo::New();
    info->conversation_id = conv_id;
    instance->RegisterConversation(std::move(info), base::DoNothing());
  }

  // Create a task.
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  tabs::TabInterface* first_tab = active_tab();
  ASSERT_TRUE(first_tab);

  // Open a second tab so we can test focusing.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  tabs::TabInterface* second_tab = active_tab();
  ASSERT_TRUE(second_tab);
  ASSERT_NE(second_tab, first_tab);

  // Make the task act on the FIRST tab.
  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  Actions action =
      ::actor::MakeNavigate(first_tab->GetHandle(), target_url.spec(), task_id);
  EXPECT_THAT(PerformActions(action),
              base::test::ValueIs(
                  HasResultCode(::actor::mojom::ActionResultCode::kOk)));

  // Now the first tab should be in LastActedTabs.

  // Call ActivateTabWithConversation.
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedService::Get(browser()->profile());
  auto result =
      glic_service->instance_coordinator().ActivateTabWithConversation(conv_id);

  EXPECT_EQ(GlicInstanceCoordinator::ActivateTabResult::kSuccess, result);

  // Verify that the FIRST tab is now active (since it was the last acted tab).
  EXPECT_EQ(active_tab(), first_tab);
}

}  // namespace
}  // namespace glic::actor

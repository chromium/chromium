// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/actor/new_glic_actor_functional_browsertest.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
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

using ::actor::ActorTask;
using ::actor::TaskId;
using ::base::test::TestFuture;
using optimization_guide::proto::Actions;

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
  GlicActorTaskLifecycleFunctionalBrowserTest()
      : GlicActorFunctionalBrowserTestBase(
            "./glic_actor_task_lifecycle_browsertest.js") {}
  ~GlicActorTaskLifecycleFunctionalBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testPauseAndResumeCreatedTask) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription completion_subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  completion_subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testPauseAndResumeInvalidTask) {
  JournalObserver observer(&actor_keyed_service()->GetJournal());

  ExecuteJsTest();

  // Pausing an invalid task should be a no-op and log an error.
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to pause task" &&
               JournalEntryHasError(entry, "No such task");
      }));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testPauseAndResumeInactiveTask) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription completion_subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  completion_subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";

  JournalObserver observer(&actor_keyed_service()->GetJournal());

  ContinueJsTest();

  // Pausing an inactive task should be a no-op and log an error.
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to pause task" &&
               JournalEntryHasError(entry, "No such task");
      }));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testPauseActiveTask) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testStopActiveTaskWithModelError) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFailed, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFailed state.";
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testInterruptAndUninterruptInvalidTask) {
  JournalObserver observer(&actor_keyed_service()->GetJournal());
  TaskId invalid_task_id = TaskId(12345);
  ASSERT_EQ(actor_keyed_service()->GetTask(invalid_task_id), nullptr);

  ExecuteJsTest();

  // Interrupting an invalid task should be a no-op and log an error.
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to interrupt task" &&
               JournalEntryHasError(entry, "No such task");
      }));

  ContinueJsTest();

  // Uninterrupting an invalid task should be a no-op and log an error.
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to uninterrupt task" &&
               JournalEntryHasError(entry, "No such task");
      }));
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testInterruptAndUninterruptTaskWithCompletedActions) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testInterruptAndUninterruptActiveTaskAndPerformActions) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testInterruptWithReasons) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testActuatingChangedCallback) {
  GlicInstanceImpl* instance = GetInstanceImpl();
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

  ExecuteJsTest();

  // After the task is created, verify the callback receives true.
  EXPECT_TRUE(actuating_true_future.Get());

  ContinueJsTest();

  // After the task is stopped, verify the callback receives false.
  EXPECT_FALSE(actuating_false_future.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testActivateTabWithConversationUsesActorState) {
  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);

  // Register a conversation ID for the instance if not present.
  std::optional<std::string> conv_id_opt = instance->conversation_id();
  std::string conv_id = conv_id_opt.value_or("test_conversation_id");
  if (!conv_id_opt.has_value()) {
    auto info = mojom::ConversationInfo::New();
    info->conversation_id = conv_id;
    instance->RegisterConversation(std::move(info), base::DoNothing());
  }

  // Execute JS test to create the task.
  ExecuteJsTest();

  tabs::TabInterface* first_tab = active_tab();
  ASSERT_TRUE(first_tab);

  // Open a second tab so we can test focusing.
  tabs::TabInterface* second_tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(second_tab);
  ASSERT_NE(second_tab, first_tab);
  EXPECT_EQ(active_tab(), second_tab);

  // Make the task act on the FIRST tab.
  ContinueJsTest({.instance = instance});

  // Now the first tab should be in LastActedTabs.

  // Call ActivateTabWithConversation.
  auto activate_result = coordinator().ActivateTabWithConversation(conv_id);

  EXPECT_EQ(GlicInstanceCoordinator::ActivateTabResult::kSuccess,
            activate_result);

  // Verify that the FIRST tab is now active (since it was the last acted tab).
  EXPECT_EQ(active_tab(), first_tab);
}

}  // namespace
}  // namespace glic::actor

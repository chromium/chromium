// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_task_options.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/fake_tool_request.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/test_support/mock_actor_ui_state_manager.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/actor/core/actor_switches.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/task_source_info.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/origin_gating/core/origin_gating_checker.h"
#include "components/origin_gating/core/task_policy_config_slot.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

using ::testing::_;

std::unique_ptr<ui::ActorUiStateManagerInterface> BuildUiStateManagerMock() {
  std::unique_ptr<ui::MockActorUiStateManager> ui_state_manager =
      std::make_unique<ui::MockActorUiStateManager>();
  ON_CALL(*ui_state_manager, OnUiEvent(_, _))
      .WillByDefault([](ui::AsyncUiEvent, ui::UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      });
  return ui_state_manager;
}

class ActorKeyedServiceTest : public testing::Test {
 public:
  ActorKeyedServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicActor);
  }
  ~ActorKeyedServiceTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager()->CreateTestingProfile("profile");
    auto* actor_service = ActorKeyedService::Get(profile());
    ASSERT_TRUE(actor_service);
    ui_state_manager_ = BuildUiStateManagerMock();
  }

  TestingProfileManager* testing_profile_manager() {
    return &testing_profile_manager_;
  }

  TestingProfile* profile() { return profile_.get(); }

  ui::ActorUiStateManagerInterface* ui_state_manager() {
    return ui_state_manager_.get();
  }

  std::unique_ptr<tabs::MockTabInterface> CreateMockTab() {
    auto mock_tab = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab, GetProfile).WillByDefault(testing::Return(profile()));
    ActorKeyedService::Get(profile())->GetSurfaceRegistry().OnTabCreated(
        *mock_tab);
    return mock_tab;
  }

  void RunTasksUntilIdle() { task_environment_.RunUntilIdle(); }
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 protected:
  base::CallbackListSubscription user_confirmation_dialog_subscription_;
  base::CallbackListSubscription confirm_navigation_subscription_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  // Lets ActorKeyedService::CreateHeadlessActorSurface() create a WebContents
  // in this unit test.
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<ui::ActorUiStateManagerInterface> ui_state_manager_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

// Adds a task to ActorKeyedService
TEST_F(ActorKeyedServiceTest, AddActiveTask) {
  auto* actor_service = ActorKeyedService::Get(profile());
  actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());
  ASSERT_EQ(actor_service->GetActiveTasks().size(), 1u);
  EXPECT_EQ(actor_service->GetActiveTasks().begin()->second->GetState(),
            ActorTask::State::kCreated);
}

// Stops a task.
TEST_F(ActorKeyedServiceTest, StopActiveTask) {
  auto* actor_service = ActorKeyedService::Get(profile());
  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());

  // Add a tab to the task
  auto mock_tab = CreateMockTab();
  const tabs::TabHandle tab_handle = mock_tab->GetHandle();
  base::WeakPtr<ActorTask> task = actor_service->GetTask(id)->GetWeakPtr();
  base::RunLoop loop;
  task->AddTab(tab_handle,
               /*stop_task_on_detach=*/true,
               base::BindLambdaForTesting([&](mojom::ActionResultPtr result) {
                 EXPECT_TRUE(IsOk(*result));
                 loop.Quit();
               }));
  loop.Run();

  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));
  actor_service->StopTask(id, ActorTask::StoppedReason::kTaskComplete);

  // Tasks are deleted asynchronously.
  EXPECT_TRUE(task);
  EXPECT_EQ(task->GetState(), ActorTask::State::kFinished);

  // Ensure the task is eventually deleted.
  WaitForPostedTask();
  ASSERT_EQ(actor_service->GetActiveTasks().size(), 0u);
  ASSERT_FALSE(task);
}

TEST_F(ActorKeyedServiceTest, FindTaskIdsInActive_ReturnsSuccessfully) {
  auto* actor_service = ActorKeyedService::Get(profile());
  actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());
  const TaskId id2 = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());
  actor_service->GetTask(id2)->Pause(/*from_actor=*/true);

  // Find a single active task.
  std::vector<TaskId> single_found =
      actor_service->FindTaskIdsInActive([](const ActorTask& task) {
        return task.GetState() == ActorTask::State::kPausedByActor;
      });
  ASSERT_EQ(single_found.size(), 1u);
  EXPECT_EQ(single_found[0], id2);
}

// Test that adding a tab to a paused or stopped task has no effect.
TEST_F(ActorKeyedServiceTest, AddTabToPausedOrStoppedTask) {
  auto* actor_service = ActorKeyedService::Get(profile());
  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());

  base::WeakPtr<ActorTask> task = actor_service->GetTask(id)->GetWeakPtr();
  ASSERT_TRUE(task);
  auto mock_tab = CreateMockTab();
  const tabs::TabHandle tab_handle = mock_tab->GetHandle();

  // Pause the task and try to add a tab.
  task->Pause(/*from_actor=*/true);
  EXPECT_TRUE(task->IsUnderUserControl());

  {
    base::RunLoop loop;
    task->AddTab(tab_handle,
                 /*stop_task_on_detach=*/true,
                 base::BindLambdaForTesting([&](mojom::ActionResultPtr result) {
                   EXPECT_EQ(result->code,
                             mojom::ActionResultCode::kTaskPaused);
                   loop.Quit();
                 }));
    loop.Run();
  }
  EXPECT_FALSE(task->IsActingOnTab(tab_handle));
  EXPECT_FALSE(task->HasTab(tab_handle));

  // Stop the task and ensure it is gone.
  actor_service->StopTask(id, ActorTask::StoppedReason::kTaskComplete);
  WaitForPostedTask();
  EXPECT_FALSE(task);
}

// Test tab association to a paused task.
TEST_F(ActorKeyedServiceTest, PausedTaskTabs) {
  auto* actor_service = ActorKeyedService::Get(profile());
  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());

  base::WeakPtr<ActorTask> task = actor_service->GetTask(id)->GetWeakPtr();
  ASSERT_TRUE(task);
  auto mock_tab = CreateMockTab();
  const tabs::TabHandle tab_handle = mock_tab->GetHandle();

  {
    base::test::TestFuture<mojom::ActionResultPtr> future;
    task->AddTab(tab_handle, /*stop_task_on_detach=*/true,
                 future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // The tab should be both part of the task and actively acting on it when in a
  // created or acting state.

  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  task->SetState(ActorTask::State::kActing);

  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  task->SetState(ActorTask::State::kReflecting);

  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  // Pausing the task should keep the tab in the task but it should no longer be
  // considered acting.

  task->Pause(true);

  EXPECT_FALSE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  task->Resume();

  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  task->Pause(false);

  EXPECT_FALSE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  task->Resume();

  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  // Stop the task. This should (asynchronously) remove the tab from the task.
  actor_service->StopTask(id, ActorTask::StoppedReason::kTaskComplete);
  WaitForPostedTask();
  EXPECT_FALSE(task);
}

TEST_F(ActorKeyedServiceTest, SetsTaskSourceInfo) {
  auto* actor_service = ActorKeyedService::Get(profile());
  const TaskSourceInfo::SourceDefinedId kId1 = "task1id";
  const TaskSourceInfo::SourceDefinedId kId2 = "task2id";

  TaskId task1 = actor_service->CreateTaskWithOptions(
      TaskSourceInfo(TaskSourceInfo::Client::kTest, kId1),
      NoEnterprisePolicyChecker(), ui_state_manager());
  TaskId task2 = actor_service->CreateTaskWithOptions(
      TaskSourceInfo(TaskSourceInfo::Client::kTest, kId2),
      NoEnterprisePolicyChecker(), ui_state_manager());

  EXPECT_EQ(actor_service->GetTask(task1)->source_info().id, kId1);
  EXPECT_EQ(actor_service->GetTask(task2)->source_info().id, kId2);
}

// Tests that GetActiveTasks() can be called from a TaskStateChangedCallback
// without crashing, even when a task is completing.
TEST_F(ActorKeyedServiceTest, GetActiveTasksDuringStateChangeCallback) {
  auto* actor_service = ActorKeyedService::Get(profile());
  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());

  bool callback_called = false;
  auto subscription = actor_service->AddTaskStateChangedCallback(
      base::BindLambdaForTesting([&](ActorTask& task) {
        if (ActorTask::IsCompletedState(task.GetState())) {
          // This should not crash. Repro for crash in http://b/493610427.
          actor_service->GetActiveTasks();
          callback_called = true;
        }
      }));

  actor_service->StopTask(id, ActorTask::StoppedReason::kTaskComplete);
  EXPECT_TRUE(callback_called);
}

TEST_F(ActorKeyedServiceTest, InitialTabAssociationOnCreate) {
  auto* actor_service = ActorKeyedService::Get(profile());
  auto mock_tab = CreateMockTab();
  const tabs::TabHandle tab_handle = mock_tab->GetHandle();
  ActorTaskOptions options;
  options.webui_task_options = webui::mojom::TaskOptions::New();
  options.webui_task_options->actuation_tab_id =
      ActorSurfaceHandle::From(tab_handle).raw_value();

  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager(),
      std::move(options));

  ActorTask* task = actor_service->GetTask(id);
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->HasTab(tab_handle));
  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_EQ(task->GetTabs().size(), 1u);
  EXPECT_TRUE(task->GetTabs().contains(tab_handle));
}

TEST_F(ActorKeyedServiceTest, InitialHeadlessSurfaceAssociationOnCreate) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ScopedHeadlessActorSurface headless =
      actor_service->CreateHeadlessActorSurface();
  const ActorSurfaceHandle handle = headless.actor_surface_handle();
  ActorTaskOptions options;
  options.webui_task_options = webui::mojom::TaskOptions::New();
  options.webui_task_options->actuation_tab_id = handle.raw_value();

  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager(),
      std::move(options));

  ActorTask* task = actor_service->GetTask(id);
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->HasActorSurface(handle));
  EXPECT_TRUE(task->IsActingOnActorSurface(handle));
}

// The journal identifies a tab by its tab ID and a headless surface, which has
// no tab, by its surface ID.
TEST_F(ActorKeyedServiceTest, JournalDetailsAddActorSurface) {
  auto mock_tab = CreateMockTab();
  const ActorSurfaceHandle tab_surface =
      ActorSurfaceHandle::From(mock_tab->GetHandle());
  std::vector<mojom::JournalDetailsPtr> tab_details =
      JournalDetailsBuilder()
          .AddActorSurface(tab_surface.raw_value(),
                           tab_surface.GetTabHandle().raw_value())
          .Build();
  ASSERT_EQ(tab_details.size(), 1u);
  EXPECT_EQ(tab_details[0]->key, "tab_id");
  EXPECT_EQ(tab_details[0]->value,
            base::ToString(mock_tab->GetHandle().raw_value()));

  ScopedHeadlessActorSurface headless =
      ActorKeyedService::Get(profile())->CreateHeadlessActorSurface();
  const ActorSurfaceHandle handle = headless.actor_surface_handle();
  std::vector<mojom::JournalDetailsPtr> headless_details =
      JournalDetailsBuilder()
          .AddActorSurface(handle.raw_value(),
                           handle.GetTabHandle().raw_value())
          .Build();
  ASSERT_EQ(headless_details.size(), 1u);
  EXPECT_EQ(headless_details[0]->key, "actor_surface_id");
  EXPECT_EQ(headless_details[0]->value, base::ToString(handle.raw_value()));
}

// `actuation_tab_id` can come from the web client, so a surface ID that another
// profile owns must not be attached to the task.
TEST_F(ActorKeyedServiceTest, InitialSurfaceFromOtherProfileNotAssociated) {
  TestingProfile* other_profile =
      testing_profile_manager()->CreateTestingProfile("other_profile");
  ScopedHeadlessActorSurface other_headless =
      ActorKeyedService::Get(other_profile)->CreateHeadlessActorSurface();
  ActorTaskOptions options;
  options.webui_task_options = webui::mojom::TaskOptions::New();
  options.webui_task_options->actuation_tab_id =
      other_headless.actor_surface_handle().raw_value();

  auto* actor_service = ActorKeyedService::Get(profile());
  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager(),
      std::move(options));

  ActorTask* task = actor_service->GetTask(id);
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetActorSurfaces().empty());
}

// ActorTask::AddActorSurface() does not attach a surface ID that resolves to no
// surface, but CreateTaskImpl() still creates the task.
TEST_F(ActorKeyedServiceTest, InitialUnknownSurfaceNotAssociated) {
  ActorTaskOptions options;
  options.webui_task_options = webui::mojom::TaskOptions::New();
  options.webui_task_options->actuation_tab_id =
      ActorSurfaceHandle::CreateForTesting().raw_value();

  auto* actor_service = ActorKeyedService::Get(profile());
  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager(),
      std::move(options));

  ActorTask* task = actor_service->GetTask(id);
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetActorSurfaces().empty());
}

// A HEADLESS task that has no initial surface gets a new headless surface.
TEST_F(ActorKeyedServiceTest, HeadlessModeCreatesHeadlessSurface) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ActorTaskOptions options;
  options.execution_mode = proto::TargetSurface::HEADLESS;
  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager(),
      std::move(options));

  ActorTask* task = actor_service->GetTask(id);
  ASSERT_TRUE(task);
  const ActorSurfaceHandle handle = task->GetLastActuatedActorSurface();
  ActorSurface* surface = actor_service->GetSurfaceRegistry().Get(handle);
  ASSERT_TRUE(surface);
  EXPECT_FALSE(surface->IsTab());
  EXPECT_TRUE(task->IsActingOnActorSurface(handle));
}

// The service destroys the headless surface that it created for a task as
// soon as the task completes.
TEST_F(ActorKeyedServiceTest, StoppingTaskDestroysItsHeadlessSurface) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ActorTaskOptions options;
  options.execution_mode = proto::TargetSurface::HEADLESS;
  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager(),
      std::move(options));
  ActorTask* task = actor_service->GetTask(id);
  ASSERT_TRUE(task);
  const ActorSurfaceHandle handle = task->GetLastActuatedActorSurface();
  ASSERT_TRUE(actor_service->GetSurfaceRegistry().Get(handle));

  actor_service->StopTask(id, ActorTask::StoppedReason::kTaskComplete);

  EXPECT_FALSE(actor_service->GetSurfaceRegistry().Get(handle));
}

// A HEADLESS task that already has an initial surface uses that surface, and
// the service does not destroy it.
TEST_F(ActorKeyedServiceTest, HeadlessModeKeepsCallerOwnedSurface) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ScopedHeadlessActorSurface headless =
      actor_service->CreateHeadlessActorSurface();
  const ActorSurfaceHandle handle = headless.actor_surface_handle();
  ActorTaskOptions options;
  options.webui_task_options = webui::mojom::TaskOptions::New();
  options.webui_task_options->actuation_tab_id = handle.raw_value();
  options.execution_mode = proto::TargetSurface::HEADLESS;
  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager(),
      std::move(options));
  ActorTask* task = actor_service->GetTask(id);
  ASSERT_TRUE(task);
  EXPECT_EQ(task->GetActorSurfaces().size(), 1u);
  EXPECT_TRUE(task->HasActorSurface(handle));

  actor_service->StopTask(id, ActorTask::StoppedReason::kTaskComplete);

  EXPECT_TRUE(actor_service->GetSurfaceRegistry().Get(handle));
}

// GetActingActorTaskForWebContents() returns a task only while one is acting on
// the surface that the WebContents backs.
TEST_F(ActorKeyedServiceTest, GetActingActorTaskForWebContents) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ScopedHeadlessActorSurface headless =
      actor_service->CreateHeadlessActorSurface();
  ScopedHeadlessActorSurface other_headless =
      actor_service->CreateHeadlessActorSurface();
  const ActorSurfaceHandle handle = headless.actor_surface_handle();
  ActorSurface* surface = actor_service->GetSurfaceRegistry().Get(handle);
  ASSERT_TRUE(surface);
  ActorSurface* other_surface = actor_service->GetSurfaceRegistry().Get(
      other_headless.actor_surface_handle());
  ASSERT_TRUE(other_surface);
  content::WebContents* web_contents = surface->GetWebContents();
  content::WebContents* other_web_contents = other_surface->GetWebContents();

  // The surfaces exist, but no task does.
  EXPECT_FALSE(actor_service->GetActingActorTaskForWebContents(web_contents));

  ActorTaskOptions options;
  options.webui_task_options = webui::mojom::TaskOptions::New();
  options.webui_task_options->actuation_tab_id = handle.raw_value();
  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager(),
      std::move(options));
  ActorTask* task = actor_service->GetTask(id);
  ASSERT_TRUE(task);
  EXPECT_EQ(actor_service->GetActingActorTaskForWebContents(web_contents),
            task);
  // The task is not acting on the other surface.
  EXPECT_FALSE(
      actor_service->GetActingActorTaskForWebContents(other_web_contents));

  actor_service->StopTask(id, ActorTask::StoppedReason::kTaskComplete);

  // The surface outlives the task, so the WebContents still backs it.
  ASSERT_TRUE(actor_service->GetSurfaceRegistry().Get(handle));
  EXPECT_FALSE(actor_service->GetActingActorTaskForWebContents(web_contents));
}

TEST_F(ActorKeyedServiceTest, TraceRecordingToFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath trace_file = temp_dir.GetPath().AppendASCII("test_trace.pb");

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
      switches::kActorTracePath, trace_file);

  TestingProfile* test_profile =
      testing_profile_manager()->CreateTestingProfile("trace_profile");
  auto* actor_service = ActorKeyedService::Get(test_profile);
  ASSERT_TRUE(actor_service);

  RunTasksUntilIdle();

  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());
  actor_service->StopTask(id, ActorTask::StoppedReason::kTaskComplete);
  testing_profile_manager()->DeleteTestingProfile("trace_profile");

  RunTasksUntilIdle();

  std::optional<int64_t> file_size = base::GetFileSize(trace_file);
  ASSERT_TRUE(file_size.has_value());
  EXPECT_GT(*file_size, 0);
}

TEST_F(ActorKeyedServiceTest, TraceRecordingToDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
      switches::kActorTracePath, temp_dir.GetPath());

  TestingProfile* test_profile =
      testing_profile_manager()->CreateTestingProfile("trace_profile_dir");
  auto* actor_service = ActorKeyedService::Get(test_profile);
  ASSERT_TRUE(actor_service);

  RunTasksUntilIdle();

  base::FilePath expected_trace_file =
      temp_dir.GetPath().AppendASCII("actor_trace.pb");

  TaskId id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());
  actor_service->StopTask(id, ActorTask::StoppedReason::kTaskComplete);
  testing_profile_manager()->DeleteTestingProfile("trace_profile_dir");

  RunTasksUntilIdle();

  std::optional<int64_t> file_size = base::GetFileSize(expected_trace_file);
  ASSERT_TRUE(file_size.has_value());
  EXPECT_GT(*file_size, 0);
}

TEST_F(ActorKeyedServiceTest, PendingTasksLifecycle) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  EXPECT_EQ(0u, actor_service->GetPendingTasksCount());
  EXPECT_FALSE(actor_service->HasPendingTask("msg-1"));

  int notification_call_count = 0;
  size_t last_notified_count = 999;
  base::CallbackListSubscription subscription =
      actor_service->AddPendingTaskCountChangedCallback(
          base::BindLambdaForTesting([&](size_t count) {
            notification_call_count++;
            last_notified_count = count;
          }));

  std::vector<std::string> added_tasks;
  base::CallbackListSubscription added_subscription =
      actor_service->AddPendingTaskAddedCallback(
          base::BindLambdaForTesting([&](const std::string& context_id) {
            added_tasks.push_back(context_id);
          }));

  // Adding a pending task fires notification.
  actor_service->AddPendingTask("msg-1");
  EXPECT_EQ(1, notification_call_count);
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());
  EXPECT_TRUE(actor_service->HasPendingTask("msg-1"));
  EXPECT_EQ(1u, last_notified_count);
  EXPECT_THAT(added_tasks, testing::ElementsAre("msg-1"));

  // Duplicate addition is ignored and does NOT fire notification.
  actor_service->AddPendingTask("msg-1");
  EXPECT_EQ(1, notification_call_count);
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());
  EXPECT_THAT(added_tasks, testing::ElementsAre("msg-1"));

  // Second pending task.
  actor_service->AddPendingTask("msg-2");
  EXPECT_EQ(2, notification_call_count);
  EXPECT_EQ(2u, actor_service->GetPendingTasksCount());
  EXPECT_EQ(2u, last_notified_count);
  EXPECT_THAT(added_tasks, testing::ElementsAre("msg-1", "msg-2"));

  // Remove first pending task.
  actor_service->RemovePendingTask("msg-1");
  EXPECT_EQ(3, notification_call_count);
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());
  EXPECT_FALSE(actor_service->HasPendingTask("msg-1"));
  EXPECT_TRUE(actor_service->HasPendingTask("msg-2"));
  EXPECT_EQ(1u, last_notified_count);

  // Remove second pending task.
  actor_service->RemovePendingTask("msg-2");
  EXPECT_EQ(4, notification_call_count);
  EXPECT_EQ(0u, actor_service->GetPendingTasksCount());
  EXPECT_EQ(0u, last_notified_count);

  // Adding without background setup updates the count but skips the added
  // callback.
  actor_service->AddPendingTaskWithoutBackgroundSetup("msg-no-notify");
  EXPECT_EQ(5, notification_call_count);
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());
  EXPECT_TRUE(actor_service->HasPendingTask("msg-no-notify"));
  EXPECT_EQ(1u, last_notified_count);
  EXPECT_THAT(added_tasks, testing::ElementsAre("msg-1", "msg-2"));

  actor_service->RemovePendingTask("msg-no-notify");
  EXPECT_EQ(6, notification_call_count);
  EXPECT_EQ(0u, actor_service->GetPendingTasksCount());
}

TEST_F(ActorKeyedServiceTest, PendingTaskRemovedOnFailureAndCancellation) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  // 1. Verify NotifyBackgroundSetupFailed removes the pending task.
  actor_service->AddPendingTask("fail-msg");
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());
  EXPECT_TRUE(actor_service->HasPendingTask("fail-msg"));

  actor_service->NotifyBackgroundSetupFailed("fail-msg");
  EXPECT_EQ(0u, actor_service->GetPendingTasksCount());
  EXPECT_FALSE(actor_service->HasPendingTask("fail-msg"));

  // 2. Verify OnMessageTriggerTaskStopped removes the pending task.
  actor_service->AddPendingTask("cancel-msg");
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());
  EXPECT_TRUE(actor_service->HasPendingTask("cancel-msg"));

  actor_service->OnMessageTriggerTaskStopped("cancel-msg");
  EXPECT_EQ(0u, actor_service->GetPendingTasksCount());
  EXPECT_FALSE(actor_service->HasPendingTask("cancel-msg"));
}

TEST_F(ActorKeyedServiceTest, ExternalTriggerTimesOutWhenNoTaskArrives) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  size_t last_notified_count = 99;
  int added_callback_count = 0;
  base::CallbackListSubscription count_subscription =
      actor_service->AddPendingTaskCountChangedCallback(
          base::BindLambdaForTesting(
              [&](size_t count) { last_notified_count = count; }));
  base::CallbackListSubscription added_subscription =
      actor_service->AddPendingTaskAddedCallback(base::BindLambdaForTesting(
          [&](const std::string&) { ++added_callback_count; }));

  actor_service->AddExternalTriggerPendingTask(base::Minutes(2));
  EXPECT_TRUE(actor_service->HasExternalTriggerPendingTaskForTesting());
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());
  EXPECT_EQ(1u, last_notified_count);
  EXPECT_EQ(0, added_callback_count);

  // Advance by 1 minute: external trigger is still within its 2-minute window.
  FastForwardBy(base::Minutes(1));
  EXPECT_TRUE(actor_service->HasExternalTriggerPendingTaskForTesting());
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());

  // Advance to 2 minutes: external trigger expires and drops pending count to
  // 0 so the foreground service stops cleanly.
  FastForwardBy(base::Minutes(1));
  EXPECT_FALSE(actor_service->HasExternalTriggerPendingTaskForTesting());
  EXPECT_EQ(0u, actor_service->GetPendingTasksCount());
  EXPECT_EQ(0u, last_notified_count);
  EXPECT_EQ(0, added_callback_count);
}

TEST_F(ActorKeyedServiceTest,
       ExternalTriggerHandsOffAtomicallyToFcmPendingTaskWithoutTimeout) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  std::vector<size_t> observed_counts;
  std::vector<std::string> provisioned_contexts;
  base::CallbackListSubscription count_subscription =
      actor_service->AddPendingTaskCountChangedCallback(
          base::BindLambdaForTesting(
              [&](size_t count) { observed_counts.push_back(count); }));
  base::CallbackListSubscription added_subscription =
      actor_service->AddPendingTaskAddedCallback(base::BindLambdaForTesting(
          [&](const std::string& id) { provisioned_contexts.push_back(id); }));

  // External Gemini intent arrives at t = 0s.
  actor_service->AddExternalTriggerPendingTask(base::Minutes(2));
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());
  EXPECT_THAT(observed_counts, testing::ElementsAre(1u));
  EXPECT_TRUE(provisioned_contexts.empty());
  EXPECT_FALSE(actor_service->HasPendingTask("fcm-context-1"));

  // FCM TriggerActuationRequest arrives at t = 5s: pending count stays at 1
  // without dropping to 0, and the external trigger timeout is cancelled.
  FastForwardBy(base::Seconds(5));
  actor_service->AddPendingTask("fcm-context-1");
  EXPECT_FALSE(actor_service->HasExternalTriggerPendingTaskForTesting());
  EXPECT_TRUE(actor_service->HasPendingTask("fcm-context-1"));
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());
  EXPECT_THAT(observed_counts, testing::ElementsAre(1u));
  EXPECT_THAT(provisioned_contexts, testing::ElementsAre("fcm-context-1"));

  // Even if WebUI loading + server planning exceeds 2 minutes, the FCM pending
  // task remains alive until explicitly resolved by Glic/server.
  FastForwardBy(base::Minutes(3));
  EXPECT_TRUE(actor_service->HasPendingTask("fcm-context-1"));
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());

  actor_service->RemovePendingTask("fcm-context-1");
  EXPECT_EQ(0u, actor_service->GetPendingTasksCount());
  EXPECT_THAT(observed_counts, testing::ElementsAre(1u, 0u));
}

TEST_F(ActorKeyedServiceTest,
       RepeatedExternalTriggerRefreshesTimeoutWithoutDoubleCounting) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  actor_service->AddExternalTriggerPendingTask(base::Minutes(2));
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());

  // Re-trigger at t = 45s resets the 2-minute window without double counting.
  FastForwardBy(base::Seconds(45));
  actor_service->AddExternalTriggerPendingTask(base::Minutes(2));
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());

  // Still pending at t = 120s (75s into second trigger).
  FastForwardBy(base::Seconds(75));
  EXPECT_TRUE(actor_service->HasExternalTriggerPendingTaskForTesting());
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());

  // Expires cleanly at t = 165s (120s after second trigger).
  FastForwardBy(base::Seconds(45));
  EXPECT_FALSE(actor_service->HasExternalTriggerPendingTaskForTesting());
  EXPECT_EQ(0u, actor_service->GetPendingTasksCount());
}

TEST_F(ActorKeyedServiceTest,
       DirectCreateTaskClearsExternalTriggerPendingTask) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  actor_service->AddExternalTriggerPendingTask(base::Minutes(2));
  EXPECT_TRUE(actor_service->HasExternalTriggerPendingTaskForTesting());
  EXPECT_EQ(1u, actor_service->GetPendingTasksCount());

  TaskId task_id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());
  EXPECT_FALSE(actor_service->HasExternalTriggerPendingTaskForTesting());
  EXPECT_EQ(0u, actor_service->GetPendingTasksCount());
  EXPECT_EQ(1u, actor_service->GetActiveTasksCount());

  actor_service->StopTask(task_id, ActorTask::StoppedReason::kTaskComplete);
  EXPECT_EQ(0u, actor_service->GetPendingTasksCount());
}

// `PerformActions()` rejects calls for an unknown task or without actions
// without running their `on_actions_accepted`. Their results are delivered
// asynchronously, which doesn't affect a later call that is accepted in the
// meantime.
TEST_F(ActorKeyedServiceTest,
       PerformActionsOnActionsAcceptedNotRunForRejectedCalls) {
  using PerformActionsFuture =
      base::test::TestFuture<std::vector<ToolResultWithLatencyInfo>,
                             TabObservationStrategy>;
  auto* actor_service = ActorKeyedService::Get(profile());
  TaskId task_id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());

  PerformActionsFuture empty_actions_result;
  actor_service->PerformActions(
      task_id, std::vector<std::unique_ptr<ToolRequest>>(), ActorTaskMetadata(),
      base::MakeExpectedNotRunClosure(FROM_HERE),
      empty_actions_result.GetCallback());

  PerformActionsFuture unknown_task_result;
  std::vector<std::unique_ptr<ToolRequest>> unknown_task_actions;
  unknown_task_actions.push_back(std::make_unique<FakeToolRequest>(
      base::BindOnce(
          [](DeprecatedToolCallback) { ADD_FAILURE() << "Tool invoked"; }),
      /*on_destroy=*/base::DoNothing()));
  actor_service->PerformActions(
      TaskId(task_id.value() + 1), std::move(unknown_task_actions),
      ActorTaskMetadata(), base::MakeExpectedNotRunClosure(FROM_HERE),
      unknown_task_result.GetCallback());

  // These actions don't run on a tab, so they are accepted right away, before
  // the rejected calls' results are delivered.
  std::vector<std::unique_ptr<ToolRequest>> actions;
  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future;
  actions.push_back(
      std::make_unique<FakeToolRequest>(on_invoke_future.GetCallback(),
                                        /*on_destroy=*/base::DoNothing()));
  base::test::TestFuture<void> accepted;
  PerformActionsFuture result;
  actor_service->PerformActions(task_id, std::move(actions),
                                ActorTaskMetadata(), accepted.GetCallback(),
                                result.GetCallback());
  EXPECT_TRUE(accepted.IsReady());
  EXPECT_FALSE(empty_actions_result.IsReady());
  EXPECT_FALSE(unknown_task_result.IsReady());

  ASSERT_TRUE(on_invoke_future.Wait());
  std::move(on_invoke_future.Take()).Run(MakeOkResult());
  ASSERT_TRUE(result.Wait());
  EXPECT_EQ(result.Get<0>()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kOk);
  EXPECT_EQ(empty_actions_result.Get<0>()[0]
                .tool_result.deprecated_action_result()
                ->code,
            mojom::ActionResultCode::kEmptyActionSequence);
  EXPECT_EQ(unknown_task_result.Get<0>()[0]
                .tool_result.deprecated_action_result()
                ->code,
            mojom::ActionResultCode::kTaskWentAway);
}

TEST_F(ActorKeyedServiceTest, CreateTaskWithOptions_PopulatesActorTaskOptions) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  auto webui_task_options = webui::mojom::TaskOptions::New();
  webui_task_options->title = "Configured Task";
  webui_task_options->duration = webui::mojom::TaskDuration::kTransient;
  webui_task_options->feature_mode =
      glic::mojom::FeatureMode::kExperimentalTriggering;

  optimization_guide::proto::AgentContainerConfig agent_container_config;
  optimization_guide::proto::Site* site =
      agent_container_config.add_location_rules()
          ->mutable_location()
          ->mutable_site();
  site->set_protocol(optimization_guide::proto::Protocol::PROTOCOL_HTTPS);
  site->set_domain("example.com");

  MockActorTaskDelegate mock_delegate;
  ActorTaskOptions options;
  options.webui_task_options = std::move(webui_task_options);
  options.delegate = mock_delegate.GetWeakPtr();
  options.glic_initial_invocation_source =
      glic::mojom::InvocationSource::kUniversalCart;
  options.agent_container_config = agent_container_config;

  TaskId task_id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager(),
      std::move(options));
  ActorTask* task = actor_service->GetTask(task_id);
  ASSERT_TRUE(task);
  EXPECT_EQ(task->title(), "Configured Task");
  EXPECT_EQ(task->delegate().get(), &mock_delegate);
  EXPECT_EQ(task->get_task_duration(), ActorTask::TaskDuration::kTransient);
  EXPECT_EQ(task->feature_mode(),
            glic::mojom::FeatureMode::kExperimentalTriggering);
  EXPECT_EQ(task->glic_initial_invocation_source(),
            glic::mojom::InvocationSource::kUniversalCart);

  const auto& slot = task->GetExecutionEngine()
                         .GetOriginGatingChecker()
                         .task_policy_config_slot();
  ASSERT_TRUE(slot.has_value());
  EXPECT_EQ(slot.value(), ConvertAgentContainerConfig(agent_container_config));
}

TEST_F(ActorKeyedServiceTest, CreateTaskWithOptions_SkipHistoryRecording) {
  auto* actor_service = ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  TaskId default_options_task_id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager());
  const ActorTask* default_options_task =
      actor_service->GetTask(default_options_task_id);
  ASSERT_TRUE(default_options_task);
  EXPECT_FALSE(default_options_task->skip_history_recording());

  ActorTaskOptions skip_options;
  skip_options.skip_history_recording = true;
  TaskId skip_task_id = actor_service->CreateTaskWithOptions(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker(), ui_state_manager(),
      std::move(skip_options));
  const ActorTask* skip_task = actor_service->GetTask(skip_task_id);
  ASSERT_TRUE(skip_task);
  EXPECT_TRUE(skip_task->skip_history_recording());
}

}  // namespace

}  // namespace actor

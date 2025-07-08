// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_state_manager.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/mock_actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/mock_event_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {
using ::tabs::MockTabInterface;
using ::tabs::TabFeatures;
using ::tabs::TabInterface;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ValuesIn;
using ui::MockUiEventDispatcher;
using ui::UiEventDispatcher;

class ActorUiStateManagerFake : public ActorUiStateManager {
 public:
  explicit ActorUiStateManagerFake(Profile* profile)
      : ActorUiStateManager(profile) {}

  void NotifyUiTabController(tabs::TabInterface& tab,
                             const UiTabState& ui_tab_state) override {
    ui_tab_state_ = ui_tab_state;
  }

  UiTabState GetUiTabState() { return ui_tab_state_; }

 private:
  UiTabState ui_tab_state_;
};

class ActorUiStateManagerTest : public testing::Test,
                                public testing::WithParamInterface<
                                    std::tuple<ActorTask::State, UiTabState>> {
 public:
  ActorUiStateManagerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ActorUiStateManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("profile");
    actor_ui_state_manager_ =
        std::make_unique<ActorUiStateManagerFake>(profile_);

    // Set up fake event dispatcher.
    std::unique_ptr<UiEventDispatcher> ui_event_dispatcher =
        ui::NewMockUiEventDispatcher();
    mock_ui_event_dispatcher_ =
        static_cast<MockUiEventDispatcher*>(ui_event_dispatcher.get());

    // Set up fake actor task.
    auto execution_engine = ExecutionEngine::CreateForTesting(
        profile(), std::move(ui_event_dispatcher));
    task_id_ = ActorKeyedService::Get(profile())->AddActiveTask(
        std::make_unique<ActorTask>(std::move(execution_engine)));
  }

  ActorUiStateManagerFake* actor_ui_state_manager() {
    return actor_ui_state_manager_.get();
  }

  void TearDown() override {
    actor_ui_state_manager_.reset();
    mock_ui_event_dispatcher_ = nullptr;
  }

  TestingProfile* profile() { return profile_.get(); }

  TaskId task_id() { return task_id_; }

  TabInterface* tab() { return &mock_tab_interface_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<ActorUiStateManagerFake> actor_ui_state_manager_;
  raw_ptr<MockUiEventDispatcher> mock_ui_event_dispatcher_;
  MockTabInterface mock_tab_interface_;
  TaskId task_id_;
};

TEST_P(ActorUiStateManagerTest, OnActorTaskState_UpdateTabScopedUi) {
  auto [task_state, expected_ui_tab_state] = GetParam();
  ActorKeyedService::Get(profile())->GetTask(task_id())->AddToTabSet(
      tab()->GetHandle());
  actor_ui_state_manager()->OnActorTaskStateChange(task_id(), task_state);
  EXPECT_EQ(actor_ui_state_manager()->GetUiTabState(), expected_ui_tab_state);
}

const auto kTestValues = std::vector<std::tuple<ActorTask::State, UiTabState>>{
    {ActorTask::State::kCreated,
     UiTabState{
         .agent_overlay = {.is_active = true},
         .handoff_button = {.is_active = true,
                            .controller =
                                HandoffButtonState::ControlOwnership::kAgent}}},
    {ActorTask::State::kActing,
     UiTabState{
         .agent_overlay = {.is_active = true},
         .handoff_button = {.is_active = true,
                            .controller =
                                HandoffButtonState::ControlOwnership::kAgent}}},
    {ActorTask::State::kReflecting,
     UiTabState{
         .agent_overlay = {.is_active = true},
         .handoff_button = {.is_active = true,
                            .controller =
                                HandoffButtonState::ControlOwnership::kAgent}}},
    {ActorTask::State::kPausedByClient,
     UiTabState{
         .agent_overlay = {.is_active = false},
         .handoff_button =
             {.is_active = true,
              .controller = HandoffButtonState::ControlOwnership::kClient}}},
    {ActorTask::State::kFinished,
     UiTabState{.agent_overlay = {.is_active = false},
                .handoff_button = {.is_active = false}}}};

INSTANTIATE_TEST_SUITE_P(ActorUiStateManagerTest,
                         ActorUiStateManagerTest,
                         ValuesIn(kTestValues));

}  // namespace
}  // namespace actor::ui

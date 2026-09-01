// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_actions_runner.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/test_support/mock_actor_ui_state_manager.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/actor/core/actor_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_task_environment.h"
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

class ActorActionsRunnerTest : public testing::Test {
 public:
  ActorActionsRunnerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicActor, actor::kGlicActorEnableScriptTools}, {});
  }
  ~ActorActionsRunnerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager()->CreateTestingProfile("profile");
    auto* actor_service = ActorKeyedService::Get(profile());
    ASSERT_TRUE(actor_service);
    actor_service->SetActorUiStateManagerForTesting(BuildUiStateManagerMock());
  }

  TestingProfileManager* testing_profile_manager() {
    return &testing_profile_manager_;
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

TEST_F(ActorActionsRunnerTest, ExecuteWaitActionSuccess) {
  optimization_guide::proto::Actions actions;
  auto* action = actions.add_actions();
  auto* wait = action->mutable_wait();
  wait->set_wait_time_ms(10);
  actions.set_skip_async_observation_collection(true);

  base::RunLoop run_loop;
  auto runner = std::make_unique<ActorActionsRunner>(
      *profile(), TestTaskSourceInfo(), std::move(actions),
      run_loop.QuitClosure());

  EXPECT_EQ(runner->result(), nullptr);
  runner->Start();

  run_loop.Run();

  std::unique_ptr<optimization_guide::proto::ActionsResult> result =
      runner->TakeResult();
  ASSERT_TRUE(result != nullptr);
  EXPECT_EQ(result->action_result(), 0);

  // Once taken, result() returns null.
  EXPECT_EQ(runner->TakeResult(), nullptr);
}

TEST_F(ActorActionsRunnerTest, TabIdInjection) {
  optimization_guide::proto::Actions actions;
  auto* script_action = actions.add_actions();
  auto* script_tool = script_action->mutable_script_tool();
  script_tool->set_tool_name("test_tool");
  script_tool->set_input_arguments("{}");
  script_tool->mutable_document_identifier()->set_serialized_token(
      base::UnguessableToken::Create().ToString());

  auto* navigate_action = actions.add_actions();
  navigate_action->mutable_navigate()->set_url("https://example.com");

  auto* translate_action = actions.add_actions();
  translate_action->mutable_translate_page()->set_target_language("fr");

  actions.set_skip_async_observation_collection(true);

  constexpr int32_t kTestTabId = 42;

  base::RunLoop run_loop;
  auto runner = std::make_unique<ActorActionsRunner>(
      *profile(), TestTaskSourceInfo(), std::move(actions),
      run_loop.QuitClosure(), kTestTabId);

  runner->Start();
  run_loop.Run();

  std::unique_ptr<optimization_guide::proto::ActionsResult> result =
      runner->TakeResult();
  ASSERT_TRUE(result != nullptr);
}

}  // namespace

}  // namespace actor

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_actuation_locality_tracker.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/actor/core/task_source_info.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(GlicActuationLocalityTrackerUnitTest, GetGeminiConversationId) {
  EXPECT_EQ(GetGeminiConversationId(actor::TaskSourceInfo(
                actor::TaskSourceInfo::Client::kGlic, "c_Conv123")),
            "c_Conv123");
  EXPECT_EQ(GetGeminiConversationId(actor::TaskSourceInfo(
                actor::TaskSourceInfo::Client::kGlic, "Conv123")),
            "Conv123");
  EXPECT_EQ(GetGeminiConversationId(actor::TaskSourceInfo(
                actor::TaskSourceInfo::Client::kGlic, std::nullopt)),
            std::nullopt);
  EXPECT_EQ(GetGeminiConversationId(actor::TaskSourceInfo(
                actor::TaskSourceInfo::Client::kGlic, "")),
            std::nullopt);
  EXPECT_EQ(GetGeminiConversationId(actor::TaskSourceInfo(
                actor::TaskSourceInfo::Client::kBrowserActuator, "c_Conv123")),
            std::nullopt);
  EXPECT_EQ(
      GetGeminiConversationId(actor::TaskSourceInfo(
          actor::TaskSourceInfo::Client::kExperimentalActor, "c_Conv123")),
      std::nullopt);
}

class FindActiveGeminiActuationTabTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kGlicActor);
    ChromeRenderViewHostTestHarness::SetUp();
    actor_service_ = static_cast<actor::ActorKeyedServiceFake*>(
        actor::ActorKeyedServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<actor::ActorKeyedServiceFake>(
                  Profile::FromBrowserContext(context));
            })));
  }

  void TearDown() override {
    actor_service_ = nullptr;
    tab_states_.clear();
    web_contents_.clear();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  actor::ActorKeyedServiceFake& actor_service() { return *actor_service_; }

  actor::TestTabState* CreateTab() {
    web_contents_.push_back(CreateTestWebContents());
    tab_states_.push_back(
        std::make_unique<actor::TestTabState>(web_contents_.back().get()));
    return tab_states_.back().get();
  }

  void DestroyTab(actor::TestTabState* target) {
    std::erase_if(tab_states_, [target](const auto& state) {
      return state.get() == target;
    });
  }

  void AddTabToTask(actor::TestTabState& tab_state,
                    actor::ActorTask& task,
                    bool stop_task_on_detach = false) {
    base::test::TestFuture<actor::mojom::ActionResultPtr> future;
    task.AddTab(tab_state.tab.GetHandle(), stop_task_on_detach,
                future.GetCallback());
    actor::ExpectOkResult(future);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<actor::ActorKeyedServiceFake> actor_service_ = nullptr;
  std::vector<std::unique_ptr<content::WebContents>> web_contents_;
  std::vector<std::unique_ptr<actor::TestTabState>> tab_states_;
};

TEST_F(FindActiveGeminiActuationTabTest, HandlesNullEmptyAndNonGlicTasks) {
  EXPECT_EQ(FindActiveGeminiActuationTab(nullptr, "c_conv-1"), nullptr);
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), ""), nullptr);
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "unknown"), nullptr);

  actor::TestTabState* tab_a = CreateTab();
  actor::TestTabState* tab_b = CreateTab();

  actor::TaskId browser_actuator_task_id =
      actor_service().CreateTaskForTesting(actor::TaskSourceInfo(
          actor::TaskSourceInfo::Client::kBrowserActuator, "c_conv-1"));
  AddTabToTask(*tab_a, *actor_service().GetTask(browser_actuator_task_id));

  actor::TaskId experimental_task_id =
      actor_service().CreateTaskForTesting(actor::TaskSourceInfo(
          actor::TaskSourceInfo::Client::kExperimentalActor, "c_conv-1"));
  AddTabToTask(*tab_b, *actor_service().GetTask(experimental_task_id));

  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "c_conv-1"), nullptr);
}

TEST_F(FindActiveGeminiActuationTabTest,
       PrefersLastActuatedTabAndFallsBackToRemainingLiveTab) {
  actor::TestTabState* tab_a = CreateTab();
  actor::TestTabState* tab_b = CreateTab();
  actor::TestTabState* tab_c = CreateTab();

  actor::TaskId task_id = actor_service().CreateTaskForTesting(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic, "c_conv-1"));
  actor::ActorTask* task = actor_service().GetTask(task_id);
  ASSERT_TRUE(task);

  // Task with no controlled tabs yet resolves to nullptr.
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "c_conv-1"), nullptr);

  AddTabToTask(*tab_a, *task);
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "c_conv-1"), &tab_a->tab);
  // Conversation ID matching is exact and opaque.
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "conv-1"), nullptr);
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "C_CONV-1"), nullptr);

  // Adding more tabs updates `last_actuated_tab_` to the latest tab (`tab_c`).
  AddTabToTask(*tab_b, *task);
  AddTabToTask(*tab_c, *task);
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "c_conv-1"), &tab_c->tab);

  // Destroying `tab_c` with two live controlled tabs remaining
  // deterministically selects the live tab with the highest `TabHandle`
  // `raw_value()`.
  DestroyTab(tab_c);
  tabs::TabInterface* expected_fallback =
      tab_b->tab.GetHandle().raw_value() > tab_a->tab.GetHandle().raw_value()
          ? &tab_b->tab
          : &tab_a->tab;
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "c_conv-1"),
            expected_fallback);

  // Destroying `tab_b` falls back to the final live controlled tab `tab_a`.
  DestroyTab(tab_b);
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "c_conv-1"), &tab_a->tab);

  // Stopping the task clears resolution.
  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "c_conv-1"), nullptr);
}

TEST_F(FindActiveGeminiActuationTabTest,
       PrefersNewestTaskWithLiveTabAndSkipsTablessOrCompletedTasks) {
  actor::TestTabState* tab_a = CreateTab();
  actor::TestTabState* tab_b = CreateTab();

  actor::TaskId task1_id =
      actor_service().CreateTaskForTesting(actor::TaskSourceInfo(
          actor::TaskSourceInfo::Client::kGlic, "c_conv-seq"));
  actor::ActorTask* task1 = actor_service().GetTask(task1_id);
  ASSERT_TRUE(task1);
  AddTabToTask(*tab_a, *task1);

  // A newer task with no tab attached yet must not shadow `task1`'s live tab.
  actor::TaskId task2_id =
      actor_service().CreateTaskForTesting(actor::TaskSourceInfo(
          actor::TaskSourceInfo::Client::kGlic, "c_conv-seq"));
  actor::ActorTask* task2 = actor_service().GetTask(task2_id);
  ASSERT_TRUE(task2);
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "c_conv-seq"), &tab_a->tab);

  // Once `task2` attaches `tab_b`, the newer task wins.
  AddTabToTask(*tab_b, *task2);
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "c_conv-seq"), &tab_b->tab);

  // Stopping `task2` falls back to the still-active `task1`.
  actor_service().StopTaskForTesting(
      task2_id, actor::ActorTask::StoppedReason::kTaskComplete);
  EXPECT_EQ(FindActiveGeminiActuationTab(profile(), "c_conv-seq"), &tab_a->tab);
}

}  // namespace
}  // namespace glic

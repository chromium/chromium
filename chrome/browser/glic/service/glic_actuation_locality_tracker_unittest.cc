// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_actuation_locality_tracker.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
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

class GlicActuationLocalityTrackerTest : public testing::Test {
 protected:
  struct TrackerTestTab {
    explicit TrackerTestTab(content::WebContents* contents)
        : tab_state(contents) {
      ON_CALL(tab_state.tab, RegisterDidInsert)
          .WillByDefault([this](tabs::TabInterface::DidInsertCallback cb) {
            return did_insert_callback_list.Add(std::move(cb));
          });
    }

    actor::TestTabState tab_state;
    base::RepeatingCallbackList<void(tabs::TabInterface*)>
        did_insert_callback_list;
  };

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile::TestingFactories testing_factories;
    testing_factories.emplace_back(
        actor::ActorKeyedServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<actor::ActorKeyedServiceFake>(
              Profile::FromBrowserContext(context));
        }));
    testing_factories.emplace_back(
        GlicKeyedServiceFactory::GetInstance(),
        base::BindRepeating(
            &GlicActuationLocalityTrackerTest::CreateGlicKeyedService,
            base::Unretained(this)));
    profile_ = profile_manager_.CreateTestingProfile(
        "test", std::move(testing_factories));
    actor_service_ = static_cast<actor::ActorKeyedServiceFake*>(
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_));
    ASSERT_TRUE(actor_service_);
    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    instance_ = std::make_unique<GlicInstanceImpl>(
        profile_, InstanceId("test-instance"),
        /*coordinator_delegate=*/nullptr,
        /*contextual_cueing_service=*/nullptr);
  }

  void TearDown() override {
    instance_.reset();
    actor_service_ = nullptr;
    tabs_.clear();
    web_contents_factory_.reset();
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

  std::unique_ptr<KeyedService> CreateGlicKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        ContextualCueingServiceFactory::GetForProfile(profile),
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
  }

  TrackerTestTab* CreateTab() {
    tabs_.push_back(std::make_unique<TrackerTestTab>(
        web_contents_factory_->CreateWebContents(profile_)));
    return tabs_.back().get();
  }

  void DestroyTab(TrackerTestTab* target) {
    std::erase_if(tabs_, [target](const auto& t) { return t.get() == target; });
  }

  void AddTabToTask(TrackerTestTab& tab, actor::ActorTask& task) {
    base::test::TestFuture<actor::mojom::ActionResultPtr> future;
    task.AddTab(tab.tab_state.tab.GetHandle(), /*stop_task_on_detach=*/false,
                future.GetCallback());
    actor::ExpectOkResult(future);
  }

  void RegisterConversation(const std::string& conversation_id) {
    auto info = mojom::ConversationInfo::New();
    info->conversation_id = conversation_id;
    instance_->RegisterConversation(std::move(info), base::DoNothing());
  }

  struct ScopedFeatureInit {
    ScopedFeatureInit() {
      feature_list.InitWithFeatures(
          {features::kGlicActor, features::kGlicActorPolicyControlExemption},
          {});
    }
    base::test::ScopedFeatureList feature_list;
  };

  ScopedFeatureInit feature_init_;
  content::BrowserTaskEnvironment task_environment_;
  GlicEnabling::ScopedBypassEnablementChecksForTesting bypass_enablement_;
  GlicProfileManager glic_profile_manager_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<actor::ActorKeyedServiceFake> actor_service_ = nullptr;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  std::vector<std::unique_ptr<TrackerTestTab>> tabs_;
  std::unique_ptr<GlicInstanceImpl> instance_;
};

TEST_F(GlicActuationLocalityTrackerTest,
       TracksRegisteredConversationAndDeduplicatesMultipleSubscribers) {
  TrackerTestTab* tab_a = CreateTab();
  std::vector<mojom::ActuationLocalityStatePtr> updates1;
  auto sub1 =
      instance_->actuation_locality_tracker().AddLocalityChangedCallback(
          base::BindRepeating(
              [](std::vector<mojom::ActuationLocalityStatePtr>* out,
                 const mojom::ActuationLocalityState& state) {
                out->push_back(state.Clone());
              },
              &updates1));

  // Unregistered instance reports `not_local` and defers actor subscriptions
  // until a conversation is registered.
  EXPECT_FALSE(instance_->actuation_locality_tracker().IsTrackingForTesting());
  EXPECT_TRUE(instance_->actuation_locality_tracker()
                  .GetCurrentState()
                  ->is_not_local());
  actor::TaskId task_id =
      actor_service_->CreateTaskForTesting(actor::TaskSourceInfo(
          actor::TaskSourceInfo::Client::kGlic, "c_conv-shared"));
  actor::ActorTask* task = actor_service_->GetTask(task_id);
  ASSERT_TRUE(task);
  AddTabToTask(*tab_a, *task);
  actor_service_->PauseTaskForTesting(task_id, /*from_actor=*/true);
  EXPECT_TRUE(updates1.empty());
  EXPECT_FALSE(instance_->actuation_locality_tracker().IsTrackingForTesting());
  EXPECT_TRUE(instance_->actuation_locality_tracker()
                  .GetCurrentState()
                  ->is_not_local());

  // Registering the matching conversation activates actor subscriptions and
  // immediately transitions to `local_tab` (non-adjacent) for subscribers.
  RegisterConversation("c_conv-shared");
  EXPECT_TRUE(instance_->actuation_locality_tracker().IsTrackingForTesting());
  ASSERT_EQ(updates1.size(), 1u);
  ASSERT_TRUE(updates1[0]->is_local_tab());
  EXPECT_FALSE(updates1[0]->get_local_tab()->is_adjacent);
  EXPECT_EQ(updates1[0]->get_local_tab()->tab_id,
            tab_a->tab_state.tab.GetHandle().raw_value());
  EXPECT_EQ(instance_->actuation_locality_tracker().GetCurrentState(),
            updates1[0]);

  // Adding a second subscriber does not re-notify `sub1`, and `GetCurrentState`
  // reflects the active state.
  std::vector<mojom::ActuationLocalityStatePtr> updates2;
  auto sub2 =
      instance_->actuation_locality_tracker().AddLocalityChangedCallback(
          base::BindRepeating(
              [](std::vector<mojom::ActuationLocalityStatePtr>* out,
                 const mojom::ActuationLocalityState& state) {
                out->push_back(state.Clone());
              },
              &updates2));
  EXPECT_EQ(updates1.size(), 1u);
  EXPECT_TRUE(updates2.empty());
  EXPECT_EQ(instance_->actuation_locality_tracker().GetCurrentState(),
            updates1[0]);

  // Stopping the task emits `not_local` once to both subscribers.
  actor_service_->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  ASSERT_EQ(updates1.size(), 2u);
  ASSERT_EQ(updates2.size(), 1u);
  EXPECT_TRUE(updates1[1]->is_not_local());
  EXPECT_EQ(updates2[0], updates1[1]);
}

TEST_F(GlicActuationLocalityTrackerTest,
       UnsubscribeCleansUpTrackingAndAllowsResubscribe) {
  TrackerTestTab* tab_a = CreateTab();
  RegisterConversation("conv-life");
  actor::TaskId task_id = actor_service_->CreateTaskForTesting(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic, "conv-life"));
  AddTabToTask(*tab_a, *actor_service_->GetTask(task_id));

  GlicActuationLocalityTracker tracker(profile_, instance_.get());
  EXPECT_FALSE(tracker.IsTrackingForTesting());
  ASSERT_TRUE(tracker.GetCurrentState()->is_local_tab());
  EXPECT_FALSE(tracker.GetCurrentState()->get_local_tab()->is_adjacent);

  std::vector<mojom::ActuationLocalityStatePtr> updates1;
  auto sub1 = tracker.AddLocalityChangedCallback(base::BindRepeating(
      [](std::vector<mojom::ActuationLocalityStatePtr>* out,
         const mojom::ActuationLocalityState& state) {
        out->push_back(state.Clone());
      },
      &updates1));
  EXPECT_TRUE(tracker.IsTrackingForTesting());
  ASSERT_TRUE(tracker.GetCurrentState()->is_local_tab());
  EXPECT_FALSE(tracker.GetCurrentState()->get_local_tab()->is_adjacent);

  // Removing the sole subscriber tears down actor subscriptions.
  sub1 = {};
  EXPECT_FALSE(tracker.IsTrackingForTesting());

  // Resubscribing re-establishes tracking and observes task stop.
  std::vector<mojom::ActuationLocalityStatePtr> updates2;
  auto sub2 = tracker.AddLocalityChangedCallback(base::BindRepeating(
      [](std::vector<mojom::ActuationLocalityStatePtr>* out,
         const mojom::ActuationLocalityState& state) {
        out->push_back(state.Clone());
      },
      &updates2));
  EXPECT_TRUE(tracker.IsTrackingForTesting());
  ASSERT_TRUE(tracker.GetCurrentState()->is_local_tab());
  EXPECT_FALSE(tracker.GetCurrentState()->get_local_tab()->is_adjacent);

  actor_service_->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  ASSERT_EQ(updates2.size(), 1u);
  EXPECT_TRUE(updates2[0]->is_not_local());
}

TEST_F(GlicActuationLocalityTrackerTest,
       IgnoresNonGeminiAndOtherConversationTasks) {
  TrackerTestTab* tab_a = CreateTab();
  RegisterConversation("c_conv-1");

  std::vector<mojom::ActuationLocalityStatePtr> updates;
  auto sub = instance_->actuation_locality_tracker().AddLocalityChangedCallback(
      base::BindRepeating(
          [](std::vector<mojom::ActuationLocalityStatePtr>* out,
             const mojom::ActuationLocalityState& state) {
            out->push_back(state.Clone());
          },
          &updates));
  EXPECT_TRUE(instance_->actuation_locality_tracker()
                  .GetCurrentState()
                  ->is_not_local());

  actor::TaskId non_gemini_id =
      actor_service_->CreateTaskForTesting(actor::TaskSourceInfo(
          actor::TaskSourceInfo::Client::kBrowserActuator, "c_conv-1"));
  AddTabToTask(*tab_a, *actor_service_->GetTask(non_gemini_id));
  actor_service_->PauseTaskForTesting(non_gemini_id, /*from_actor=*/true);
  actor_service_->StopTaskForTesting(
      non_gemini_id, actor::ActorTask::StoppedReason::kTaskComplete);

  actor::TaskId other_conv_id =
      actor_service_->CreateTaskForTesting(actor::TaskSourceInfo(
          actor::TaskSourceInfo::Client::kGlic, "conv-other"));
  AddTabToTask(*tab_a, *actor_service_->GetTask(other_conv_id));
  actor_service_->PauseTaskForTesting(other_conv_id, /*from_actor=*/true);
  actor_service_->StopTaskForTesting(
      other_conv_id, actor::ActorTask::StoppedReason::kTaskComplete);

  EXPECT_TRUE(updates.empty());
  EXPECT_TRUE(instance_->actuation_locality_tracker()
                  .GetCurrentState()
                  ->is_not_local());
}

TEST_F(GlicActuationLocalityTrackerTest,
       TabMoveDoesNotEmitNotLocalWhileTabDeleteDoes) {
  TrackerTestTab* tab_a = CreateTab();
  RegisterConversation("conv-tab");
  actor::TaskId task_id = actor_service_->CreateTaskForTesting(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic, "conv-tab"));
  AddTabToTask(*tab_a, *actor_service_->GetTask(task_id));

  std::vector<mojom::ActuationLocalityStatePtr> updates;
  auto sub = instance_->actuation_locality_tracker().AddLocalityChangedCallback(
      base::BindRepeating(
          [](std::vector<mojom::ActuationLocalityStatePtr>* out,
             const mojom::ActuationLocalityState& state) {
            out->push_back(state.Clone());
          },
          &updates));
  ASSERT_TRUE(instance_->actuation_locality_tracker()
                  .GetCurrentState()
                  ->is_local_tab());

  // Moving a tab between windows fires `WillDetach(kInsertIntoOtherWindow)` +
  // `DidInsert` without transitioning to `not_local`.
  tab_a->tab_state.will_detach_callback_list_.Notify(
      &tab_a->tab_state.tab,
      tabs::TabInterface::DetachReason::kInsertIntoOtherWindow);
  tab_a->did_insert_callback_list.Notify(&tab_a->tab_state.tab);
  EXPECT_TRUE(updates.empty());

  // Firing `WillDetach(kDelete)` while `tab_a` is still alive immediately emits
  // `not_local` via `detaching_tab_handles_` independently of `~TabInterface`.
  // When destruction does not follow, the posted cleanup task clears the
  // transient handle and restores `local_tab`.
  tab_a->tab_state.will_detach_callback_list_.Notify(
      &tab_a->tab_state.tab, tabs::TabInterface::DetachReason::kDelete);
  ASSERT_TRUE(base::test::RunUntil([&] { return updates.size() == 2u; }));
  EXPECT_TRUE(updates[0]->is_not_local());
  ASSERT_TRUE(updates[1]->is_local_tab());
  EXPECT_FALSE(updates[1]->get_local_tab()->is_adjacent);
  EXPECT_EQ(updates[1]->get_local_tab()->tab_id,
            tab_a->tab_state.tab.GetHandle().raw_value());

  // Destroying the tab fires `WillDetach(kDelete)` and deletes `tab_a`,
  // transitioning permanently to `not_local`.
  DestroyTab(tab_a);
  ASSERT_EQ(updates.size(), 3u);
  EXPECT_TRUE(updates[2]->is_not_local());
}

TEST_F(GlicActuationLocalityTrackerTest,
       TracksAdjacentTabAndTransitionsOnVisibilityChange) {
  TrackerTestTab* tab_a = CreateTab();
  TrackerTestTab* tab_b = CreateTab();
  RegisterConversation("conv-adj");
  actor::TaskId task_id = actor_service_->CreateTaskForTesting(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic, "conv-adj"));
  AddTabToTask(*tab_a, *actor_service_->GetTask(task_id));

  instance_->SetActiveEmbedderForTesting(
      SidePanelEmbedderKey(tab_a->tab_state.tab));

  std::vector<mojom::ActuationLocalityStatePtr> updates;
  auto sub = instance_->actuation_locality_tracker().AddLocalityChangedCallback(
      base::BindRepeating(
          [](std::vector<mojom::ActuationLocalityStatePtr>* out,
             const mojom::ActuationLocalityState& state) {
            out->push_back(state.Clone());
          },
          &updates));
  const auto initial =
      instance_->actuation_locality_tracker().GetCurrentState();
  ASSERT_TRUE(initial->is_local_tab());
  EXPECT_TRUE(initial->get_local_tab()->is_adjacent);
  EXPECT_EQ(initial->get_local_tab()->tab_id,
            tab_a->tab_state.tab.GetHandle().raw_value());

  // Moving the active side panel to another tab flips `is_adjacent` to false
  // via `OnInstanceVisibilityChanged`.
  instance_->SetActiveEmbedderForTesting(
      SidePanelEmbedderKey(tab_b->tab_state.tab));
  ASSERT_EQ(updates.size(), 1u);
  ASSERT_TRUE(updates[0]->is_local_tab());
  EXPECT_FALSE(updates[0]->get_local_tab()->is_adjacent);
  EXPECT_EQ(updates[0]->get_local_tab()->tab_id,
            tab_a->tab_state.tab.GetHandle().raw_value());

  // Switching to floating mode remains non-adjacent and is deduplicated.
  instance_->SetActiveEmbedderForTesting(FloatingEmbedderKey{});
  EXPECT_EQ(updates.size(), 1u);

  // Returning the active side panel to `tab_a` restores `is_adjacent = true`.
  instance_->SetActiveEmbedderForTesting(
      SidePanelEmbedderKey(tab_a->tab_state.tab));
  ASSERT_EQ(updates.size(), 2u);
  ASSERT_TRUE(updates[1]->is_local_tab());
  EXPECT_TRUE(updates[1]->get_local_tab()->is_adjacent);
  EXPECT_EQ(updates[1]->get_local_tab()->tab_id,
            tab_a->tab_state.tab.GetHandle().raw_value());

  // Clearing the active embedder flips `is_adjacent` back to false.
  instance_->SetActiveEmbedderForTesting(std::nullopt);
  ASSERT_EQ(updates.size(), 3u);
  ASSERT_TRUE(updates[2]->is_local_tab());
  EXPECT_FALSE(updates[2]->get_local_tab()->is_adjacent);
  EXPECT_EQ(updates[2]->get_local_tab()->tab_id,
            tab_a->tab_state.tab.GetHandle().raw_value());

  // Stopping the task transitions to `not_local`.
  actor_service_->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  ASSERT_EQ(updates.size(), 4u);
  EXPECT_TRUE(updates[3]->is_not_local());
}

TEST_F(GlicActuationLocalityTrackerTest,
       RefreshesOnTaskSurfaceChangesWithoutStateToggle) {
  TrackerTestTab* tab_a = CreateTab();
  TrackerTestTab* tab_b = CreateTab();
  RegisterConversation("conv-hook");

  std::vector<mojom::ActuationLocalityStatePtr> updates;
  auto sub = instance_->actuation_locality_tracker().AddLocalityChangedCallback(
      base::BindRepeating(
          [](std::vector<mojom::ActuationLocalityStatePtr>* out,
             const mojom::ActuationLocalityState& state) {
            out->push_back(state.Clone());
          },
          &updates));
  EXPECT_TRUE(instance_->actuation_locality_tracker()
                  .GetCurrentState()
                  ->is_not_local());

  // Creating a task with no tabs stays `not_local`; adding `tab_a` immediately
  // transitions to `local_tab` via `AddTaskSurfacesChangedCallback()` without
  // pausing or toggling visibility.
  actor::TaskId task_id = actor_service_->CreateTaskForTesting(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic, "conv-hook"));
  actor::ActorTask* task = actor_service_->GetTask(task_id);
  ASSERT_TRUE(task);
  EXPECT_TRUE(updates.empty());

  AddTabToTask(*tab_a, *task);
  ASSERT_EQ(updates.size(), 1u);
  ASSERT_TRUE(updates[0]->is_local_tab());
  EXPECT_FALSE(updates[0]->get_local_tab()->is_adjacent);
  EXPECT_EQ(updates[0]->get_local_tab()->tab_id,
            tab_a->tab_state.tab.GetHandle().raw_value());

  // Adding a second tab `tab_b` without any state change updates tab ID.
  AddTabToTask(*tab_b, *task);
  ASSERT_EQ(updates.size(), 2u);
  ASSERT_TRUE(updates[1]->is_local_tab());
  EXPECT_EQ(updates[1]->get_local_tab()->tab_id,
            tab_b->tab_state.tab.GetHandle().raw_value());

  // Re-adding already-controlled `tab_a` updates `last_actuated_tab_` ->
  // `tab_a`.
  AddTabToTask(*tab_a, *task);
  ASSERT_EQ(updates.size(), 3u);
  ASSERT_TRUE(updates[2]->is_local_tab());
  EXPECT_EQ(updates[2]->get_local_tab()->tab_id,
            tab_a->tab_state.tab.GetHandle().raw_value());

  // Removing `tab_a` falls back to `tab_b` without waiting for state change.
  task->RemoveTab(tab_a->tab_state.tab.GetHandle());
  ASSERT_EQ(updates.size(), 4u);
  ASSERT_TRUE(updates[3]->is_local_tab());
  EXPECT_EQ(updates[3]->get_local_tab()->tab_id,
            tab_b->tab_state.tab.GetHandle().raw_value());

  // Removing the final tab transitions to `not_local`.
  task->RemoveTab(tab_b->tab_state.tab.GetHandle());
  ASSERT_EQ(updates.size(), 5u);
  EXPECT_TRUE(updates[4]->is_not_local());
}

TEST_F(GlicActuationLocalityTrackerTest,
       RefreshesWhenRegisterConversationBackfillsPendingTaskSourceId) {
  TrackerTestTab* tab_a = CreateTab();
  std::vector<mojom::ActuationLocalityStatePtr> updates;
  auto sub = instance_->actuation_locality_tracker().AddLocalityChangedCallback(
      base::BindRepeating(
          [](std::vector<mojom::ActuationLocalityStatePtr>* out,
             const mojom::ActuationLocalityState& state) {
            out->push_back(state.Clone());
          },
          &updates));
  EXPECT_TRUE(instance_->actuation_locality_tracker()
                  .GetCurrentState()
                  ->is_not_local());

  // Create a task through `GlicActorTaskManager` before registering a
  // conversation so the task starts without a `source_info().id` and is queued
  // for backfill.
  ASSERT_TRUE(instance_->GetActorTaskManager());
  mojo::Remote<mojom::ActorHandler> actor_handler;
  mojo::PendingReceiver<mojom::ActorClient> actor_client_receiver;
  instance_->GetActorTaskManager()->Bind(
      actor_handler.BindNewPipeAndPassReceiver(),
      actor_client_receiver.InitWithNewPipeAndPassRemote());
  base::ScopedClosureRunner unbind_session(
      base::BindOnce(&GlicActorTaskManager::UnbindSession,
                     base::Unretained(instance_->GetActorTaskManager())));

  base::test::TestFuture<base::expected<int32_t, mojom::CreateTaskErrorReason>>
      create_future;
  actor_handler->CreateTask(/*task_options=*/nullptr,
                            create_future.GetCallback());
  ASSERT_TRUE(create_future.Get().has_value());
  actor::ActorTask* task =
      actor_service_->GetTask(actor::TaskId(create_future.Get().value()));
  ASSERT_TRUE(task);
  EXPECT_FALSE(task->source_info().id.has_value());

  // Do not pump the run loop after `AddTab()`: the real
  // `GlicActorClientSession` delegate posts
  // `GlicInstanceImpl::OnTabAddedToTask`, which expects a full tab helper
  // (`GlicInstanceHelper`) not attached to `MockTabInterface`.
  task->AddTab(tab_a->tab_state.tab.GetHandle(), /*stop_task_on_detach=*/false,
               base::DoNothing());
  ASSERT_TRUE(task->HasTab(tab_a->tab_state.tab.GetHandle()));
  EXPECT_TRUE(updates.empty());

  // `GlicInstanceImpl::RegisterConversation()` backfills `task->SetSourceId()`
  // before notifying `OnInstanceConversationRegistered()`, so the tracker
  // immediately picks up `tab_a` without a separate `SetSourceId` hook.
  RegisterConversation("conv-backfill");
  EXPECT_EQ(task->source_info().id, "conv-backfill");
  ASSERT_EQ(updates.size(), 1u);
  ASSERT_TRUE(updates[0]->is_local_tab());
  EXPECT_FALSE(updates[0]->get_local_tab()->is_adjacent);
  EXPECT_EQ(updates[0]->get_local_tab()->tab_id,
            tab_a->tab_state.tab.GetHandle().raw_value());
}

}  // namespace
}  // namespace glic

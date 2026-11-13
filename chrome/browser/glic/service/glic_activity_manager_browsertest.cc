// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/service/glic_activity_manager.h"

#include <string_view>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_activity_types.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/task_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

// Matches `ResponseStopCause` values in `glic_api.ts`.
constexpr int kClientResponseStopCauseUser = 0;
constexpr int kClientResponseStopCauseOther = 1;

class GlicActivityManagerBrowserTest : public GlicBrowserTest {
 public:
  GlicActivityManagerBrowserTest() {
    SetGlicPagePath("/glic/test_client/index.html");
    feature_list_.InitWithFeatures(
        {features::kGlicActor, features::kGlicActorPolicyControlExemption,
         features::kGlicActivityHub},
        {});
  }

 protected:
  GlicActivityManager& activity_manager() {
    return service()->activity_manager();
  }

  [[nodiscard]] testing::AssertionResult RegisterConversationInClient(
      GlicInstanceImpl* instance,
      std::string_view conversation_id,
      std::string_view title) {
    return content::ExecJs(instance->host().web_client_contents(),
                           content::JsReplace(
                               R"js(
                                 window.client.browser.registerConversation({
                                   conversationId: $1,
                                   conversationTitle: $2,
                                 })
                               )js",
                               conversation_id, title));
  }

  [[nodiscard]] testing::AssertionResult NotifyUserInputSubmittedInClient(
      GlicInstanceImpl* instance) {
    return content::ExecJs(instance->host().web_client_contents(),
                           R"js(
          window.client.browser.getMetrics().onUserInputSubmitted(0);
        )js");
  }

  [[nodiscard]] testing::AssertionResult NotifyResponseStartedInClient(
      GlicInstanceImpl* instance) {
    return content::ExecJs(instance->host().web_client_contents(),
                           R"js(
          window.client.browser.getMetrics().onResponseStarted();
        )js");
  }

  [[nodiscard]] testing::AssertionResult NotifyResponseStoppedInClient(
      GlicInstanceImpl* instance) {
    return content::ExecJs(instance->host().web_client_contents(),
                           R"js(
          window.client.browser.getMetrics().onResponseStopped();
        )js");
  }

  [[nodiscard]] testing::AssertionResult NotifyResponseStoppedWithCauseInClient(
      GlicInstanceImpl* instance,
      int client_stop_cause) {
    return content::ExecJs(instance->host().web_client_contents(),
                           content::JsReplace(
                               R"js(
              window.client.browser.getMetrics().onResponseStopped({
                cause: $1,
              });
              window.client.browser.getMetrics().onTurnCompleted(0, 100);
            )js",
                               client_stop_cause));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActivityManagerBrowserTest,
                       StreamsBackgroundConversationTurnAndMarksSeenOnReopen) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  ASSERT_TRUE(RegisterConversationInClient(instance, "conv_1", "Trip Planner"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance));

  // Hide the panel so the conversation runs in the background.
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose(instance));

  ASSERT_TRUE(NotifyResponseStartedInClient(instance));
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().HasUnseenOrInProgressActivities(); },
      true));

  const GlicActivityKey key{GlicActivityType::kConversationResponse, "conv_1"};
  auto it = activity_manager().activities().find(key);
  ASSERT_NE(it, activity_manager().activities().end());
  EXPECT_EQ(it->second.state, GlicActivityState::kInProgress);
  EXPECT_EQ(it->second.title, u"Trip Planner");
  EXPECT_EQ(activity_manager().GetNumUnseenActivities(), 0u);

  base::test::TestFuture<ActivityInterruptionLevel> alert_future;
  base::CallbackListSubscription alert_sub =
      activity_manager().RegisterActivityAlertCallback(
          alert_future.GetRepeatingCallback());

  ASSERT_TRUE(NotifyResponseStoppedInClient(instance));
  EXPECT_EQ(alert_future.Get(), ActivityInterruptionLevel::kPassiveNudge);

  it = activity_manager().activities().find(key);
  ASSERT_NE(it, activity_manager().activities().end());
  EXPECT_EQ(it->second.state, GlicActivityState::kCompleted);
  EXPECT_FALSE(it->second.has_been_seen);
  EXPECT_EQ(activity_manager().GetNumUnseenActivities(), 1u);
  EXPECT_EQ(activity_manager().GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kResponseReady);

  // Re-opening the panel marks the background response as seen via
  // GlicInstanceObserver::OnInstanceVisibilityChanged.
  ASSERT_OK_AND_ASSIGN(instance, OpenGlicForActiveTab());
  EXPECT_EQ(activity_manager().GetNumUnseenActivities(), 0u);
  it = activity_manager().activities().find(key);
  ASSERT_NE(it, activity_manager().activities().end());
  EXPECT_TRUE(it->second.has_been_seen);
}

IN_PROC_BROWSER_TEST_F(GlicActivityManagerBrowserTest,
                       BuffersResponseStartedBeforeConversationRegistration) {
  base::UserActionTester user_action_tester;
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance));
  ASSERT_TRUE(NotifyResponseStartedInClient(instance));
  ASSERT_OK(WaitForUserActionCount(user_action_tester, "GlicResponseStart", 1));

  // Buffered per InstanceId until a conversation ID is registered.
  EXPECT_TRUE(activity_manager().activities().empty());

  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose(instance));

  ASSERT_TRUE(
      RegisterConversationInClient(instance, "conv_buffered", "Buffered Chat"));

  const GlicActivityKey key{GlicActivityType::kConversationResponse,
                            "conv_buffered"};
  auto it = activity_manager().activities().find(key);
  ASSERT_NE(it, activity_manager().activities().end());
  EXPECT_EQ(it->second.state, GlicActivityState::kInProgress);
  EXPECT_EQ(it->second.title, u"Buffered Chat");

  ASSERT_TRUE(NotifyResponseStoppedInClient(instance));
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().GetNumUnseenActivities(); }, 1u));
}

IN_PROC_BROWSER_TEST_F(
    GlicActivityManagerBrowserTest,
    TransitionsOutOfInProgressWhenResponseStoppedByUserOrError) {
  base::HistogramTester histogram_tester;
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  ASSERT_TRUE(
      RegisterConversationInClient(instance, "conv_stop", "Stopped Chat"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance));

  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose(instance));

  // User-initiated stop clears in-progress activity even when followed by
  // onTurnCompleted.
  ASSERT_TRUE(NotifyResponseStartedInClient(instance));
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().HasUnseenOrInProgressActivities(); },
      true));

  ASSERT_TRUE(NotifyResponseStoppedWithCauseInClient(
      instance, kClientResponseStopCauseUser));
  ASSERT_OK(RunUntilEqual(
      [&]() {
        return histogram_tester.GetBucketCount("Glic.Turn.Duration.Default",
                                               100);
      },
      1));
  EXPECT_FALSE(activity_manager().HasUnseenOrInProgressActivities());
  const GlicActivityKey key{GlicActivityType::kConversationResponse,
                            "conv_stop"};
  auto it = activity_manager().activities().find(key);
  ASSERT_NE(it, activity_manager().activities().end());
  EXPECT_EQ(it->second.state, GlicActivityState::kIdle);

  // Error/system stop transitions the background activity to kFailed.
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance));
  ASSERT_TRUE(NotifyResponseStartedInClient(instance));
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().HasUnseenOrInProgressActivities(); },
      true));

  base::test::TestFuture<ActivityInterruptionLevel> alert_future;
  base::CallbackListSubscription alert_sub =
      activity_manager().RegisterActivityAlertCallback(
          alert_future.GetRepeatingCallback());

  ASSERT_TRUE(NotifyResponseStoppedWithCauseInClient(
      instance, kClientResponseStopCauseOther));
  EXPECT_EQ(alert_future.Get(), ActivityInterruptionLevel::kPassiveNudge);
  ASSERT_OK(RunUntilEqual(
      [&]() {
        return histogram_tester.GetBucketCount("Glic.Turn.Duration.Default",
                                               100);
      },
      2));

  it = activity_manager().activities().find(key);
  ASSERT_NE(it, activity_manager().activities().end());
  EXPECT_EQ(it->second.state, GlicActivityState::kFailed);
  EXPECT_EQ(activity_manager().GetNumUnseenActivities(), 1u);
}

IN_PROC_BROWSER_TEST_F(GlicActivityManagerBrowserTest,
                       ClearsInProgressActivityWhenWebClientCleared) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  ASSERT_TRUE(
      RegisterConversationInClient(instance, "conv_abort", "Aborted Turn"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance));

  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose(instance));

  ASSERT_TRUE(NotifyResponseStartedInClient(instance));
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().HasUnseenOrInProgressActivities(); },
      true));

  // Reloading the guest disconnects the prior WebClientHandler Mojo pipe and
  // triggers GlicInstanceObserver::OnInstanceWebClientCleared, resetting the
  // live conversation back to kIdle. Wait for the reloaded client to finish
  // reconnecting so GuestView teardown is clean.
  ASSERT_TRUE(content::ExecJs(instance->host().web_client_contents(),
                              "window.location.reload();"));
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().HasUnseenOrInProgressActivities(); },
      false));
  const GlicActivityKey key{GlicActivityType::kConversationResponse,
                            "conv_abort"};
  auto it = activity_manager().activities().find(key);
  ASSERT_NE(it, activity_manager().activities().end());
  EXPECT_EQ(it->second.state, GlicActivityState::kIdle);
  ASSERT_OK(WaitForGlicClient(instance));
}

IN_PROC_BROWSER_TEST_F(GlicActivityManagerBrowserTest,
                       RemovesConversationActivityWhenInstanceDestroyed) {
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  ASSERT_TRUE(RegisterConversationInClient(instance, "conv_closing",
                                           "Closing Tab Chat"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance));

  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose(instance));

  ASSERT_TRUE(NotifyResponseStartedInClient(instance));
  ASSERT_TRUE(NotifyResponseStoppedInClient(instance));
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().GetNumUnseenActivities(); }, 1u));

  base::WeakPtr<GlicInstanceImpl> weak_instance = instance->GetWeakPtr();
  GetTabListInterface()->CloseTab(tab2->GetHandle());
  ASSERT_OK(WaitForInstanceDeletion(weak_instance));

  EXPECT_TRUE(activity_manager().activities().empty());
  EXPECT_EQ(activity_manager().GetNumUnseenActivities(), 0u);
}

IN_PROC_BROWSER_TEST_F(GlicActivityManagerBrowserTest,
                       TracksIdleConversationAcrossTabsEvenWhenPanelClosed) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance1, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance1));
  ASSERT_TRUE(
      RegisterConversationInClient(instance1, "conv_bg", "Background Chat"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance1));

  const GlicActivityKey key{GlicActivityType::kConversationResponse, "conv_bg"};
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().activities().contains(key); }, true));

  tabs::TabInterface* tab2 = CreateBackgroundTab(GURL("about:blank"));

  // While open on tab1, conv_bg is filtered out for tab1 and included for tab2.
  EXPECT_TRUE(activity_manager().GetSortedActivities(tab1).empty());
  std::vector<const GlicActivityItem*> tab2_items =
      activity_manager().GetSortedActivities(tab2);
  ASSERT_EQ(tab2_items.size(), 1u);
  EXPECT_EQ(tab2_items[0]->key.id, "conv_bg");
  EXPECT_EQ(tab2_items[0]->state, GlicActivityState::kIdle);
  EXPECT_EQ(tab2_items[0]->title, u"Background Chat");

  // Closing the panel on tab1 means conv_bg is no longer showing on tab1, so it
  // is included for both tab1 and tab2.
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose(instance1));

  std::vector<const GlicActivityItem*> tab1_items =
      activity_manager().GetSortedActivities(tab1);
  ASSERT_EQ(tab1_items.size(), 1u);
  EXPECT_EQ(tab1_items[0]->key.id, "conv_bg");
  tab2_items = activity_manager().GetSortedActivities(tab2);
  ASSERT_EQ(tab2_items.size(), 1u);
  EXPECT_EQ(tab2_items[0]->key.id, "conv_bg");

  // Closing tab1 destroys the instance and clears the conversation for tab2.
  base::WeakPtr<GlicInstanceImpl> weak_instance = instance1->GetWeakPtr();
  ActivateTab(tab2);
  GetTabListInterface()->CloseTab(tab1->GetHandle());
  ASSERT_OK(WaitForInstanceDeletion(weak_instance));

  EXPECT_TRUE(activity_manager().activities().empty());
  EXPECT_TRUE(activity_manager().GetSortedActivities(tab2).empty());
}

IN_PROC_BROWSER_TEST_F(
    GlicActivityManagerBrowserTest,
    TracksMixedActorTaskAndBackgroundConversationsAcrossTabs) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance1, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance1));
  ASSERT_TRUE(
      RegisterConversationInClient(instance1, "conv_1", "Flight Search"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance1));
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose(instance1));

  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance2, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance2));
  ASSERT_TRUE(
      RegisterConversationInClient(instance2, "conv_2", "Hotel Options"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance2));

  auto task_options = actor::webui::mojom::TaskOptions::New();
  task_options->title = "Book Hotel";
  task_options->duration = actor::webui::mojom::TaskDuration::kDefault;
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  actor::TaskId task_id = actor_service->CreateTaskWithOptions(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker(),
      std::move(task_options), nullptr,
      actor::ui::ActorUiStateManager::Get(GetProfile()));
  actor::AddTabToTask(*tab1, *actor_service->GetTask(task_id));
  actor_service->GetTask(task_id)->Pause(/*from_actor=*/true);

  const std::string task_id_str = base::NumberToString(task_id.value());
  const GlicActivityKey task_key{GlicActivityType::kActorTask, task_id_str};
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().activities().contains(task_key); },
      true));

  // While instance2 is open on tab2, conv_2 is filtered out for tab2 while the
  // attention-needing Actor task sorts ahead of idle conv_1.
  std::vector<const GlicActivityItem*> tab2_items =
      activity_manager().GetSortedActivities(tab2);
  ASSERT_EQ(tab2_items.size(), 2u);
  EXPECT_EQ(tab2_items[0]->type(), GlicActivityType::kActorTask);
  EXPECT_EQ(tab2_items[0]->key.id, task_id_str);
  EXPECT_EQ(tab2_items[0]->state, GlicActivityState::kNeedsAttention);
  EXPECT_EQ(tab2_items[0]->title, u"Book Hotel");
  EXPECT_EQ(tab2_items[1]->key.id, "conv_1");
  EXPECT_EQ(tab2_items[1]->state, GlicActivityState::kIdle);

  // Start a background response on conv_1 and close instance2 on tab2 so all
  // three activities are visible and ordered by priority.
  ASSERT_TRUE(NotifyResponseStartedInClient(instance1));
  const GlicActivityKey conv1_key{GlicActivityType::kConversationResponse,
                                  "conv_1"};
  ASSERT_OK(RunUntilEqual(
      [&]() {
        auto it = activity_manager().activities().find(conv1_key);
        return it != activity_manager().activities().end() &&
               it->second.state == GlicActivityState::kInProgress;
      },
      true));

  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose(instance2));

  for (tabs::TabInterface* tab : {tab1, tab2}) {
    std::vector<const GlicActivityItem*> items =
        activity_manager().GetSortedActivities(tab);
    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(items[0]->key.id, task_id_str);
    EXPECT_EQ(items[0]->state, GlicActivityState::kNeedsAttention);
    EXPECT_EQ(items[1]->key.id, "conv_1");
    EXPECT_EQ(items[1]->state, GlicActivityState::kInProgress);
    EXPECT_EQ(items[2]->key.id, "conv_2");
    EXPECT_EQ(items[2]->state, GlicActivityState::kIdle);
  }
}

// Glic floaty is not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicActivityManagerBrowserTest,
                       ExcludesDetachedFloatyConversationAcrossAllTabs) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_TRUE(
      RegisterConversationInClient(instance, "conv_floaty", "Floating Chat"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance));

  const GlicActivityKey key{GlicActivityType::kConversationResponse,
                            "conv_floaty"};
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().activities().contains(key); }, true));

  tabs::TabInterface* unbound_tab = CreateAndActivateTab(GURL("about:blank"));

  // Because conv_floaty is open in a floaty, it is excluded regardless of which
  // tab is active.
  EXPECT_TRUE(activity_manager().GetSortedActivities(tab1).empty());
  EXPECT_TRUE(activity_manager().GetSortedActivities(unbound_tab).empty());
  EXPECT_TRUE(activity_manager().GetSortedActivities(nullptr).empty());

  // Once the floaty is closed, conv_floaty is included across all tabs.
  instance->CloseAllEmbedders();
  ASSERT_OK(WaitForGlicClose(instance));

  std::vector<const GlicActivityItem*> tab1_items =
      activity_manager().GetSortedActivities(tab1);
  ASSERT_EQ(tab1_items.size(), 1u);
  EXPECT_EQ(tab1_items[0]->key.id, "conv_floaty");

  std::vector<const GlicActivityItem*> unbound_items =
      activity_manager().GetSortedActivities(unbound_tab);
  ASSERT_EQ(unbound_items.size(), 1u);
  EXPECT_EQ(unbound_items[0]->key.id, "conv_floaty");
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace glic

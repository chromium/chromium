// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/service/glic_activity_manager.h"

#include <string_view>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_activity_types.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
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
    feature_list_.InitAndEnableFeature(features::kGlicActivityHub);
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

}  // namespace
}  // namespace glic

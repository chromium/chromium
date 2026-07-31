// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/new_glic_api_test.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/sharing_message/mock_sharing_message_sender.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "crypto/keypair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace glic {

namespace {

constexpr int64_t kDefaultSequenceNumber = 42;
constexpr char kDefaultServerConfig[] = "test_config";
constexpr char kDefaultP256dh[] = "test_p256dh";
constexpr char kDefaultAuthSecret[] = "test_auth_secret";

components_sharing_message::SharingMessage CreateTriggeringMessage(
    int64_t sequence_number = kDefaultSequenceNumber,
    const std::string& server_config = kDefaultServerConfig) {
  components_sharing_message::SharingMessage message;
  auto* channel = message.mutable_server_channel_configuration();
  channel->set_configuration(server_config);
  channel->set_p256dh(kDefaultP256dh);
  channel->set_auth_secret(kDefaultAuthSecret);
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->mutable_task_metadata()->set_sender_sequence_number(
      sequence_number);
  // Set the current version by default to test the version check success path.
  triggering->set_glic_experimental_triggering_version(1);
  return message;
}

}  // namespace

using testing::_;

class MockGlicExperimentalTriggeringMessageHandler
    : public GlicExperimentalTriggeringMessageHandler {
 public:
  MockGlicExperimentalTriggeringMessageHandler(Profile* profile,
                                               SharingMessageSender* sender)
      : GlicExperimentalTriggeringMessageHandler(profile, sender) {}
  MOCK_METHOD(BrowserWindowInterface*, GetBrowserWindow, (), (const, override));
};

class GlicExperimentalTriggeringMessageHandlerBrowserTest
    : public GlicApiBrowserTest {
 public:
  GlicExperimentalTriggeringMessageHandlerBrowserTest()
      : GlicApiBrowserTest(
            "./glic_experimental_triggering_message_handler_browsertest.js") {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicExperimentalTriggering, {}},
         {features::kGlicExperimentalTriggeringScreenshot, {}},
         {features::kGlicActor,
          {{features::kGlicActorPolicyControlExemption.name, "true"}}},
         {features::kGlicExperimentalTriggeringOptInTabFocus,
          {{"glic-experimental-triggering-tab-focus-hosts",
            "127.0.0.1,localhost"},
           {"glic-experimental-triggering-tab-focus-path-substring",
            "/test_data"}}}},
        {});
  }
  ~GlicExperimentalTriggeringMessageHandlerBrowserTest() override = default;

#if !BUILDFLAG(IS_ANDROID)
  using PlatformBrowserTest::browser;
#endif

 protected:
  void SetUpOnMainThread() override {
    GlicApiBrowserTest::SetUpOnMainThread();
    GlicEnabling::SetBypassEnablementChecksForTesting(true);

    // Mark enterprise management authority for platform and profile as NONE
    // to avoid ambient management state on some bots affecting tests.
    platform_management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForPlatform(),
            policy::EnterpriseManagementAuthority::NONE);
    profile_management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(GetProfile()),
            policy::EnterpriseManagementAuthority::NONE);

    handler_ = std::make_unique<GlicExperimentalTriggeringMessageHandler>(
        GetProfile(), &mock_sharing_message_sender_);

    ASSERT_TRUE(content::NavigateToURL(
        GetTabListInterface()->GetActiveTab()->GetContents(),
        GetTestUrl("page.html")));
  }

  void OptIn() {
    auto* glic_service = glic::GlicKeyedService::Get(GetProfile());
    ASSERT_TRUE(glic_service);
    glic_service->enabling().SetCompletedFre(
        glic::prefs::FreStatus::kCompleted);
    glic_service->enabling().SetUserEnabledActuationOnWeb(true);
    glic_service->enabling().SetExperimentalTriggeringEnabled(true);
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    platform_management_override_.reset();
    profile_management_override_.reset();
    GlicApiBrowserTest::TearDownOnMainThread();
  }

  void SetupMessageSenderMock(
      base::test::TestFuture<
          components_sharing_message::ServerChannelConfiguration,
          components_sharing_message::SharingMessage>* future) {
    EXPECT_CALL(mock_sharing_message_sender_,
                SendMessageToServerTarget(_, _, _, _))
        .WillOnce(
            [future](
                const components_sharing_message::ServerChannelConfiguration&
                    server_channel,
                base::TimeDelta timeout,
                components_sharing_message::SharingMessage message,
                SharingMessageSender::ResponseCallback callback) {
              future->SetValue(server_channel, std::move(message));
              return base::OnceClosure();
            });
  }

  std::unique_ptr<components_sharing_message::ResponseMessage>
  SendMessageAndWait(components_sharing_message::SharingMessage message) {
    base::test::TestFuture<
        std::unique_ptr<components_sharing_message::ResponseMessage>>
        done_future;
    handler_->OnMessage(std::move(message), done_future.GetCallback());
    EXPECT_TRUE(done_future.Wait());
    return done_future.Take();
  }

  base::test::ScopedFeatureList feature_list_;
  testing::NiceMock<MockSharingMessageSender> mock_sharing_message_sender_;
  std::unique_ptr<GlicExperimentalTriggeringMessageHandler> handler_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      platform_management_override_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      profile_management_override_;
};

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testGetExperimentalTriggeringUpdates) {
  OptIn();
  base::HistogramTester histogram_tester;
  auto message = CreateTriggeringMessage(101);
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  int initial_tab_count = GetTabListInterface()->GetTabCount();
  int initial_active_index = GetTabListInterface()->GetActiveIndex();

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      future;
  SetupMessageSenderMock(&future);

  SendMessageAndWait(std::move(message));

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.StateOnActuationRequest",
      syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kSuccess, 1);

  // Verify that a new tab was created and it is in the background.
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), initial_tab_count + 1);
  EXPECT_EQ(GetTabListInterface()->GetActiveIndex(), initial_active_index);

  // Verify that the instance is bound to the newly created tab.
  auto* new_tab = GetTabListInterface()->GetTab(initial_tab_count);
  ASSERT_TRUE(new_tab);
  ASSERT_OK(WaitForGlicInstanceBoundToTab(new_tab));

  ExecuteJsTest();

  auto [server_channel, received_message] = future.Take();
  EXPECT_EQ(server_channel.configuration(), "test_config");
  EXPECT_EQ(server_channel.p256dh(), kDefaultP256dh);
  EXPECT_EQ(server_channel.auth_secret(), kDefaultAuthSecret);
  EXPECT_FALSE(received_message.has_server_channel_configuration());
  EXPECT_TRUE(received_message.has_glic_experimental_triggering());
  EXPECT_FALSE(
      received_message.glic_experimental_triggering().context_id().empty());
  EXPECT_TRUE(received_message.glic_experimental_triggering().has_response());
  EXPECT_TRUE(received_message.glic_experimental_triggering()
                  .response()
                  .has_task_update());
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .response()
                .task_update()
                .state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::COMPLETE);
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            1);
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            101);
}

class GlicExperimentalTriggeringMetadataEnabledBrowserTest
    : public GlicExperimentalTriggeringMessageHandlerBrowserTest {
 public:
  GlicExperimentalTriggeringMetadataEnabledBrowserTest() {
    metadata_feature_list_.InitAndEnableFeature(
        features::kGlicStructuredYieldMetadata);
  }

 private:
  base::test::ScopedFeatureList metadata_feature_list_;
};

class GlicExperimentalTriggeringMetadataDisabledBrowserTest
    : public GlicExperimentalTriggeringMessageHandlerBrowserTest {
 public:
  GlicExperimentalTriggeringMetadataDisabledBrowserTest() {
    metadata_feature_list_.InitAndDisableFeature(
        features::kGlicStructuredYieldMetadata);
  }

 private:
  base::test::ScopedFeatureList metadata_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMetadataEnabledBrowserTest,
                       testRelaysUpdatesWithMetadataEnabled) {
  OptIn();

  auto message = CreateTriggeringMessage(101);
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      future;
  SetupMessageSenderMock(&future);

  SendMessageAndWait(std::move(message));

  ExecuteJsTest();

  auto [server_channel, received_message] = future.Take();
  EXPECT_TRUE(received_message.has_glic_experimental_triggering());
  EXPECT_TRUE(received_message.glic_experimental_triggering().has_response());
  EXPECT_TRUE(received_message.glic_experimental_triggering()
                  .response()
                  .has_task_update());
  const auto& task_update =
      received_message.glic_experimental_triggering().response().task_update();
  EXPECT_EQ(task_update.state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::RUNNING);
  EXPECT_EQ(task_update.data(), "test_update_with_metadata");

  // Verify that metadata is populated.
  EXPECT_EQ(task_update.metadata_size(), 2);
  EXPECT_EQ(task_update.metadata().at("key1"), "value1");
  EXPECT_EQ(task_update.metadata().at("key2"), "value2");
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMetadataDisabledBrowserTest,
                       testRelaysUpdatesWithMetadataDisabled) {
  OptIn();

  auto message = CreateTriggeringMessage(101);
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      future;
  SetupMessageSenderMock(&future);

  SendMessageAndWait(std::move(message));

  ExecuteJsTest();

  auto [server_channel, received_message] = future.Take();
  EXPECT_TRUE(received_message.has_glic_experimental_triggering());
  EXPECT_TRUE(received_message.glic_experimental_triggering().has_response());
  EXPECT_TRUE(received_message.glic_experimental_triggering()
                  .response()
                  .has_task_update());
  const auto& task_update =
      received_message.glic_experimental_triggering().response().task_update();
  EXPECT_EQ(task_update.state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::RUNNING);
  EXPECT_EQ(task_update.data(), "test_update_with_metadata");

  // Verify that metadata is empty.
  EXPECT_EQ(task_update.metadata_size(), 0);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testLatestServerChannelConfigurationWinsForUpdates) {
  OptIn();

  auto message1 = CreateTriggeringMessage(42, "config_1");
  auto* triggering1 = message1.mutable_glic_experimental_triggering();
  triggering1->set_context_id("test-context-id");
  triggering1->mutable_request()->mutable_device_opt_in_request();

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      update_future;

  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillRepeatedly(
          [&](const components_sharing_message::ServerChannelConfiguration&
                  server_channel,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            update_future.SetValue(server_channel, std::move(message));
            return base::OnceClosure();
          });

  handler_->OnMessage(std::move(message1), base::DoNothing());

  auto message2 = CreateTriggeringMessage(43, "config_2");
  auto* triggering2 = message2.mutable_glic_experimental_triggering();
  triggering2->set_context_id("test-context-id");
  triggering2->mutable_request()->mutable_device_opt_in_request();

  handler_->OnMessage(std::move(message2), base::DoNothing());

  // Close the active tab to trigger dialog teardown/rejection.
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  auto [server_channel, received_message] = update_future.Take();
  EXPECT_EQ(server_channel.configuration(), "config_2");
}
#endif

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testRelaysUpdatesWithSequenceNumbers) {
  OptIn();
  auto message = CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  base::test::TestFuture<components_sharing_message::SharingMessage> future(
      base::test::TestFutureMode::kQueue);
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillRepeatedly(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(std::move(message));
            return base::OnceClosure();
          });

  SendMessageAndWait(std::move(message));

  // Verify that the instance is bound to the newly created tab.
  auto* new_tab = GetTabListInterface()->GetTab(initial_tab_count);
  ASSERT_TRUE(new_tab);
  ASSERT_OK(WaitForGlicInstanceBoundToTab(new_tab));

  ExecuteJsTest();

  auto message1 = future.Take();
  EXPECT_FALSE(message1.has_server_channel_configuration());
  EXPECT_EQ(message1.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            1);
  EXPECT_EQ(message1.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            42);

  auto message2 = future.Take();
  EXPECT_FALSE(message2.has_server_channel_configuration());
  EXPECT_EQ(message2.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            2);
  EXPECT_EQ(message2.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            42);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testRelaysResumedUpdate) {
  OptIn();
  auto message = CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  base::test::TestFuture<components_sharing_message::SharingMessage> future;
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillOnce(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(std::move(message));
            return base::OnceClosure();
          });

  SendMessageAndWait(std::move(message));

  ExecuteJsTest();

  auto received_message = future.Take();
  ASSERT_TRUE(received_message.glic_experimental_triggering().has_response());
  const auto& response =
      received_message.glic_experimental_triggering().response();
  ASSERT_TRUE(response.has_task_update());
  const auto& task_update = response.task_update();
  EXPECT_EQ(task_update.state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::RESUMED);
  EXPECT_FALSE(task_update.has_data_type());
  EXPECT_EQ(task_update.data(), "");
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testRespectsLastSeenSequenceNumber) {
  OptIn();
  auto message = CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  base::test::TestFuture<components_sharing_message::SharingMessage> future;
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillOnce(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(std::move(message));
            return base::OnceClosure();
          });

  SendMessageAndWait(std::move(message));

  // Verify that the instance is bound to the newly created tab.
  auto* new_tab = GetTabListInterface()->GetTab(initial_tab_count);
  ASSERT_TRUE(new_tab);
  ASSERT_OK(WaitForGlicInstanceBoundToTab(new_tab));

  ExecuteJsTest();

  auto received_message = future.Take();
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            1);
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            kDefaultSequenceNumber);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testRelaysConversationId) {
  OptIn();
  auto message = CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  base::test::TestFuture<components_sharing_message::SharingMessage> future;
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillOnce(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(std::move(message));
            return base::OnceClosure();
          });

  SendMessageAndWait(std::move(message));

  auto* new_tab = GetTabListInterface()->GetTab(initial_tab_count);
  ASSERT_TRUE(new_tab);
  ASSERT_OK(WaitForGlicInstanceBoundToTab(new_tab));

  ExecuteJsTest();

  auto received_message = future.Take();
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .conversation_id(),
            "test_conv_id");
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            1);
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            kDefaultSequenceNumber);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testDuplicateTriggerMessageReturnsErrorWithoutUaf) {
  OptIn();
  const std::string context_id = "test_duplicate_trigger_context";
  const std::string conversation_id = "test_conv_id";

  auto message1 = CreateTriggeringMessage(101);
  message1.mutable_glic_experimental_triggering()->set_context_id(context_id);
  message1.mutable_glic_experimental_triggering()
      ->mutable_task_metadata()
      ->set_conversation_id(conversation_id);
  message1.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  base::test::TestFuture<components_sharing_message::SharingMessage> future;
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillOnce(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(std::move(message));
            return base::OnceClosure();
          });

  auto response1 = SendMessageAndWait(std::move(message1));
  ASSERT_TRUE(response1);
  EXPECT_TRUE(response1->has_glic_experimental_triggering());
  EXPECT_EQ(response1->glic_experimental_triggering()
                .response()
                .task_update()
                .state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::STARTING);

  // Send a second trigger message with the same context_id and conversation_id
  // while the first invocation is still in progress. This targets the existing
  // active invocation and drives the synchronous kInvokeInProgress error path
  // in InvokeWithAutoSubmit.
  auto message2 = CreateTriggeringMessage(102);
  message2.mutable_glic_experimental_triggering()->set_context_id(context_id);
  message2.mutable_glic_experimental_triggering()
      ->mutable_task_metadata()
      ->set_conversation_id(conversation_id);
  message2.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  auto response2 = SendMessageAndWait(std::move(message2));
  ASSERT_TRUE(response2);
  EXPECT_TRUE(response2->has_glic_experimental_triggering());
  EXPECT_EQ(response2->glic_experimental_triggering()
                .response()
                .task_update()
                .state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::STARTING);

  // Verify that an outgoing message with state FAILED was emitted synchronously
  // by options.on_error due to kInvokeInProgress without UAF.
  auto update_message = future.Take();
  EXPECT_EQ(update_message.glic_experimental_triggering()
                .response()
                .task_update()
                .state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::FAILED);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       HandlesDeviceOptInRequest) {
  auto message = CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_device_opt_in_request()
      ->set_triggering_source("ChromeOS");

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      future;
  SetupMessageSenderMock(&future);

  SendMessageAndWait(std::move(message));

  auto* glic_service = glic::GlicKeyedService::Get(GetProfile());
  ASSERT_TRUE(glic_service);
  glic_service->opt_in_controller().CloseDialog(/*accepted=*/true);

  auto [server_channel, received_message] = future.Take();
  EXPECT_EQ(server_channel.configuration(), "test_config");
  EXPECT_TRUE(received_message.has_glic_experimental_triggering());
  EXPECT_TRUE(received_message.glic_experimental_triggering().has_response());
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .response()
                .device_opt_in_result(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::ACCEPTED);
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            0);
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            kDefaultSequenceNumber);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       HandlesMultipleConcurrentDeviceOptInRequests) {
  auto message1 = CreateTriggeringMessage(42, "test_config_1");
  message1.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_device_opt_in_request()
      ->set_triggering_source("ChromeOS");

  auto message2 = CreateTriggeringMessage(43, "test_config_2");
  message2.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_device_opt_in_request()
      ->set_triggering_source("ChromeOS");

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      future1;
  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      future2;

  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillOnce(
          [&](const components_sharing_message::ServerChannelConfiguration&
                  server_channel,
              base::TimeDelta timeout,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback callback) {
            future1.SetValue(server_channel, std::move(message));
            return base::OnceClosure();
          })
      .WillOnce(
          [&](const components_sharing_message::ServerChannelConfiguration&
                  server_channel,
              base::TimeDelta timeout,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback callback) {
            future2.SetValue(server_channel, std::move(message));
            return base::OnceClosure();
          });

  SendMessageAndWait(std::move(message1));
  SendMessageAndWait(std::move(message2));

  auto* glic_service = glic::GlicKeyedService::Get(GetProfile());
  ASSERT_TRUE(glic_service);
  glic_service->opt_in_controller().CloseDialog(/*accepted=*/true);

  auto [server_channel1, received_message1] = future1.Take();
  EXPECT_EQ(server_channel1.configuration(), "test_config_1");
  EXPECT_TRUE(received_message1.has_glic_experimental_triggering());
  EXPECT_TRUE(received_message1.glic_experimental_triggering().has_response());
  EXPECT_EQ(received_message1.glic_experimental_triggering()
                .response()
                .device_opt_in_result(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::ACCEPTED);
  EXPECT_EQ(received_message1.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            0);
  EXPECT_EQ(received_message1.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            42);

  auto [server_channel2, received_message2] = future2.Take();
  EXPECT_EQ(server_channel2.configuration(), "test_config_2");
  EXPECT_TRUE(received_message2.has_glic_experimental_triggering());
  EXPECT_TRUE(received_message2.glic_experimental_triggering().has_response());
  EXPECT_EQ(received_message2.glic_experimental_triggering()
                .response()
                .device_opt_in_result(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::ACCEPTED);
  EXPECT_EQ(received_message2.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            0);
  EXPECT_EQ(received_message2.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            43);
}

IN_PROC_BROWSER_TEST_F(
    GlicExperimentalTriggeringMessageHandlerBrowserTest,
    HandlesMultipleConcurrentDeviceOptInRequestsDeclinedOnTeardown) {
  auto message1 = CreateTriggeringMessage(42, "test_config_1");
  message1.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_device_opt_in_request()
      ->set_triggering_source("ChromeOS");

  auto message2 = CreateTriggeringMessage(43, "test_config_2");
  message2.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_device_opt_in_request()
      ->set_triggering_source("ChromeOS");

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      future1;
  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      future2;

  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillOnce(
          [&](const components_sharing_message::ServerChannelConfiguration&
                  server_channel,
              base::TimeDelta timeout,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback callback) {
            future1.SetValue(server_channel, std::move(message));
            return base::OnceClosure();
          })
      .WillOnce(
          [&](const components_sharing_message::ServerChannelConfiguration&
                  server_channel,
              base::TimeDelta timeout,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback callback) {
            future2.SetValue(server_channel, std::move(message));
            return base::OnceClosure();
          });

  SendMessageAndWait(std::move(message1));
  SendMessageAndWait(std::move(message2));

  // Close the active tab to trigger dialog teardown/rejection.
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  auto [server_channel1, received_message1] = future1.Take();
  EXPECT_EQ(server_channel1.configuration(), "test_config_1");
  EXPECT_TRUE(received_message1.has_glic_experimental_triggering());
  EXPECT_TRUE(received_message1.glic_experimental_triggering().has_response());
  EXPECT_EQ(received_message1.glic_experimental_triggering()
                .response()
                .device_opt_in_result(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::DECLINED);
  EXPECT_EQ(received_message1.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            0);
  EXPECT_EQ(received_message1.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            42);

  auto [server_channel2, received_message2] = future2.Take();
  EXPECT_EQ(server_channel2.configuration(), "test_config_2");
  EXPECT_TRUE(received_message2.has_glic_experimental_triggering());
  EXPECT_TRUE(received_message2.glic_experimental_triggering().has_response());
  EXPECT_EQ(received_message2.glic_experimental_triggering()
                .response()
                .device_opt_in_result(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::DECLINED);
  EXPECT_EQ(received_message2.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            0);
  EXPECT_EQ(received_message2.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            43);
}
#endif

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testHandlesStartAndStopActuationRequestsSuccessfully) {
  OptIn();
  // --- Step 1: Start Actuation ---
  auto start_message = CreateTriggeringMessage(kDefaultSequenceNumber);
  auto* start_triggering = start_message.mutable_glic_experimental_triggering();
  start_triggering->set_context_id("test-context-id");
  start_triggering->mutable_request()->mutable_trigger_actuation_request();

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  base::test::TestFuture<components_sharing_message::SharingMessage> future;
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillRepeatedly(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(std::move(message));
            return base::OnceClosure();
          });

  auto start_response = SendMessageAndWait(std::move(start_message));
  ASSERT_TRUE(start_response);
  EXPECT_TRUE(start_response->has_glic_experimental_triggering());
  EXPECT_EQ(start_response->glic_experimental_triggering()
                .response()
                .task_update()
                .state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::STARTING);
  EXPECT_FALSE(start_response->glic_experimental_triggering()
                   .response()
                   .task_update()
                   .has_data_type());

  // Verify that the instance is bound to the newly created tab.
  auto* new_tab = GetTabListInterface()->GetTab(initial_tab_count);
  ASSERT_TRUE(new_tab);
  ASSERT_OK(WaitForGlicInstanceBoundToTab(new_tab));

  ExecuteJsTest();

  // --- Step 2: Stop Actuation ---
  auto stop_message = CreateTriggeringMessage(kDefaultSequenceNumber + 1);
  auto* stop_triggering = stop_message.mutable_glic_experimental_triggering();
  stop_triggering->set_context_id("test-context-id");
  stop_triggering->mutable_request()
      ->mutable_stop_actuation_request()
      ->set_stop_reason("STOPPED_BY_USER");

  auto response = SendMessageAndWait(std::move(stop_message));
  ASSERT_TRUE(response);
  EXPECT_TRUE(response->has_glic_experimental_triggering());
  EXPECT_EQ(response->glic_experimental_triggering().context_id(),
            "test-context-id");
  EXPECT_EQ(
      response->glic_experimental_triggering().response().task_update().state(),
      components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringResponse::TaskUpdate::STOPPED);
  EXPECT_FALSE(response->glic_experimental_triggering()
                   .response()
                   .task_update()
                   .has_data_type());
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            1);
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            kDefaultSequenceNumber + 1);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testHandlesContinueActuationRequestSuccessfully) {
  OptIn();
  // --- Step 1: Start Actuation ---
  auto start_message = CreateTriggeringMessage(kDefaultSequenceNumber);
  auto* start_triggering = start_message.mutable_glic_experimental_triggering();
  start_triggering->set_context_id("test-context-id");
  start_triggering->mutable_request()->mutable_trigger_actuation_request();

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  base::test::TestFuture<components_sharing_message::SharingMessage> future;
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillRepeatedly(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(std::move(message));
            return base::OnceClosure();
          });

  auto start_response = SendMessageAndWait(std::move(start_message));
  ASSERT_TRUE(start_response);
  EXPECT_TRUE(start_response->has_glic_experimental_triggering());
  EXPECT_EQ(start_response->glic_experimental_triggering()
                .response()
                .task_update()
                .state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::STARTING);

  // Verify that the instance is bound to the newly created tab.
  auto* new_tab = GetTabListInterface()->GetTab(initial_tab_count);
  ASSERT_TRUE(new_tab);
  ASSERT_OK(WaitForGlicInstanceBoundToTab(new_tab));

  ExecuteJsTest();

  // --- Step 2: Continue Actuation ---
  auto continue_message = CreateTriggeringMessage(kDefaultSequenceNumber + 1);
  auto* continue_triggering =
      continue_message.mutable_glic_experimental_triggering();
  continue_triggering->set_context_id("test-context-id");
  continue_triggering->mutable_request()
      ->mutable_continue_actuation_request()
      ->set_continuation_prompt("continuation prompt");

  auto response = SendMessageAndWait(std::move(continue_message));
  ASSERT_TRUE(response);
  EXPECT_TRUE(response->has_glic_experimental_triggering());
  EXPECT_EQ(response->glic_experimental_triggering().context_id(),
            "test-context-id");
  EXPECT_EQ(
      response->glic_experimental_triggering().response().task_update().state(),
      components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringResponse::TaskUpdate::STARTING);
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            1);
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            kDefaultSequenceNumber + 1);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testContinueActuationTargetsCorrectTab) {
  OptIn();
  // --- Step 1: Start Actuation ---
  auto start_message = CreateTriggeringMessage(kDefaultSequenceNumber);
  auto* start_triggering = start_message.mutable_glic_experimental_triggering();
  start_triggering->set_context_id("test-context-id");
  start_triggering->mutable_request()->mutable_trigger_actuation_request();

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  base::test::TestFuture<components_sharing_message::SharingMessage> future;
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillRepeatedly(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(std::move(message));
            return base::OnceClosure();
          });

  auto start_response = SendMessageAndWait(std::move(start_message));
  ASSERT_TRUE(start_response);

  auto* tab2 = GetTabListInterface()->GetTab(initial_tab_count);
  ASSERT_TRUE(tab2);
  ASSERT_OK(WaitForGlicInstanceBoundToTab(tab2));

  ExecuteJsTest();

  auto response_msg = future.Take();
  std::string conversation_id = response_msg.glic_experimental_triggering()
                                    .task_metadata()
                                    .conversation_id();
  ASSERT_FALSE(conversation_id.empty());

  // Switch back to Tab 1 (making it active)
  GetTabListInterface()->ActivateTab(
      GetTabListInterface()->GetTab(0)->GetHandle());

  // --- Step 2: Continue Actuation ---
  auto continue_message = CreateTriggeringMessage(kDefaultSequenceNumber + 1);
  auto* continue_triggering =
      continue_message.mutable_glic_experimental_triggering();
  continue_triggering->set_context_id("test-context-id");
  continue_triggering->mutable_task_metadata()->set_conversation_id(
      conversation_id);
  auto* continue_req = continue_triggering->mutable_request()
                           ->mutable_continue_actuation_request();
  continue_req->set_continuation_prompt("continuation prompt");

  auto response = SendMessageAndWait(std::move(continue_message));
  ASSERT_TRUE(response);

  // Verify that no new tab was created (we still only have Tab 1 and Tab 2)
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), initial_tab_count + 1);

  EXPECT_EQ(response->glic_experimental_triggering().context_id(),
            "test-context-id");
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            2);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       RejectRequestWhenNotOptedIn) {
  // Ensure we are NOT opted in.
  auto* glic_service = glic::GlicKeyedService::Get(GetProfile());
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetCompletedFre(glic::prefs::FreStatus::kNotStarted);
  glic_service->enabling().SetUserEnabledActuationOnWeb(false);
  glic_service->enabling().SetExperimentalTriggeringEnabled(false);

  auto message = CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .Times(0);

  auto response = SendMessageAndWait(std::move(message));

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.StateOnActuationRequest",
      syncer::DeviceInfo::GlicExperimentalTriggeringState::kNeedsOptIn, 1);

  // Verify that Glic was NOT invoked (no new tabs created).
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 1);

  // Verify that a FAILED response was sent back with the correct error message.
  ASSERT_TRUE(response);
  EXPECT_TRUE(response->has_glic_experimental_triggering());
  EXPECT_FALSE(response->glic_experimental_triggering().context_id().empty());
  EXPECT_EQ(
      response->glic_experimental_triggering().response().task_update().state(),
      components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringResponse::TaskUpdate::FAILED);
  EXPECT_EQ(response->glic_experimental_triggering()
                .response()
                .task_update()
                .data_type(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::ERROR_MESSAGE);
  EXPECT_EQ(
      response->glic_experimental_triggering().response().task_update().data(),
      "User is not opted in to experimental triggering.");
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            0);
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            kDefaultSequenceNumber);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       CleansUpUpdatesHandlerOnPayloadNotSet) {
  auto message = CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->mutable_request();  // Empty request, payload is NOT_SET

  SendMessageAndWait(std::move(message));

  // Handler map should be empty.
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       CleansUpUpdatesHandlerOnOptInRejection) {
  // Ensure we are NOT opted in.
  auto* glic_service = glic::GlicKeyedService::Get(GetProfile());
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetCompletedFre(glic::prefs::FreStatus::kNotStarted);
  glic_service->enabling().SetUserEnabledActuationOnWeb(false);
  glic_service->enabling().SetExperimentalTriggeringEnabled(false);

  auto message = CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .Times(0);

  auto response = SendMessageAndWait(std::move(message));
  ASSERT_TRUE(response);
  EXPECT_TRUE(response->has_glic_experimental_triggering());
  EXPECT_FALSE(response->glic_experimental_triggering().context_id().empty());
  EXPECT_EQ(
      response->glic_experimental_triggering().response().task_update().state(),
      components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringResponse::TaskUpdate::FAILED);
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            0);
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            kDefaultSequenceNumber);

  // Response sent, and handler should be cleaned up immediately.
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       CleansUpUpdatesHandlerOnDeviceOptIn) {
  auto message = CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_device_opt_in_request()
      ->set_triggering_source("ChromeOS");

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      future;
  SetupMessageSenderMock(&future);

  SendMessageAndWait(std::move(message));

  // The updates handler should be created and stored in the map while waiting
  // for opt-in dialog.
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  auto* glic_service = glic::GlicKeyedService::Get(GetProfile());
  ASSERT_TRUE(glic_service);
  glic_service->opt_in_controller().CloseDialog(/*accepted=*/true);

  auto [server_channel, received_message] = future.Take();
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            0);
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            kDefaultSequenceNumber);

  // After opt-in completes, the updates handler should be erased.
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}
#endif

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       CleansUpUpdatesHandlerOnStopActuation) {
  OptIn();
  // Start Actuation
  auto start_message = CreateTriggeringMessage();
  auto* start_triggering = start_message.mutable_glic_experimental_triggering();
  start_triggering->set_context_id("test-context-id");
  start_triggering->mutable_request()->mutable_trigger_actuation_request();

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  base::test::TestFuture<components_sharing_message::SharingMessage> future;
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillRepeatedly(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(std::move(message));
            return base::OnceClosure();
          });

  auto start_response = SendMessageAndWait(std::move(start_message));
  ASSERT_TRUE(start_response);
  EXPECT_TRUE(start_response->has_glic_experimental_triggering());
  EXPECT_EQ(start_response->glic_experimental_triggering()
                .response()
                .task_update()
                .state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::STARTING);
  EXPECT_FALSE(start_response->glic_experimental_triggering()
                   .response()
                   .task_update()
                   .has_data_type());

  // Active triggering handler should exist.
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  // Verify the instance is bound.
  auto* new_tab = GetTabListInterface()->GetTab(initial_tab_count);
  ASSERT_TRUE(new_tab);
  ASSERT_OK(WaitForGlicInstanceBoundToTab(new_tab));

  // Stop Actuation
  auto stop_message = CreateTriggeringMessage(kDefaultSequenceNumber + 1);
  auto* stop_triggering = stop_message.mutable_glic_experimental_triggering();
  stop_triggering->set_context_id("test-context-id");
  stop_triggering->mutable_request()
      ->mutable_stop_actuation_request()
      ->set_stop_reason("STOPPED_BY_USER");

  auto response = SendMessageAndWait(std::move(stop_message));
  ASSERT_TRUE(response);
  EXPECT_TRUE(response->has_glic_experimental_triggering());
  EXPECT_EQ(
      response->glic_experimental_triggering().response().task_update().state(),
      components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringResponse::TaskUpdate::STOPPED);
  EXPECT_FALSE(response->glic_experimental_triggering()
                   .response()
                   .task_update()
                   .has_data_type());
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            1);
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            kDefaultSequenceNumber + 1);

  // The stop request completes, and the handler should be cleaned up.
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testRelaysParentConversationMetadataUpdated) {
  OptIn();
  auto start_message = CreateTriggeringMessage();
  auto* start_triggering = start_message.mutable_glic_experimental_triggering();
  start_triggering->set_context_id("test-context-id");
  start_triggering->mutable_request()->mutable_trigger_actuation_request();

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  base::test::TestFuture<components_sharing_message::SharingMessage> future;
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillRepeatedly(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(std::move(message));
            return base::OnceClosure();
          });

  SendMessageAndWait(std::move(start_message));

  auto* new_tab = GetTabListInterface()->GetTab(initial_tab_count);
  ASSERT_TRUE(new_tab);
  ASSERT_OK(WaitForGlicInstanceBoundToTab(new_tab));

  ExecuteJsTest();

  auto update_message = CreateTriggeringMessage();
  auto* update_triggering =
      update_message.mutable_glic_experimental_triggering();
  update_triggering->set_context_id("test-context-id");
  update_triggering->mutable_task_metadata_updated();
  auto* parent_metadata = update_triggering->mutable_task_metadata()
                              ->mutable_parent_conversation_metadata();
  parent_metadata->set_conversation_id("test_conv_id");
  parent_metadata->set_conversation_title("test_title");

  SendMessageAndWait(std::move(update_message));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testRelaysParentConversationMetadataInitial) {
  OptIn();
  auto start_message = CreateTriggeringMessage();
  auto* start_triggering = start_message.mutable_glic_experimental_triggering();
  start_triggering->set_context_id("test-context-id");
  start_triggering->mutable_request()->mutable_trigger_actuation_request();
  auto* parent_metadata = start_triggering->mutable_task_metadata()
                              ->mutable_parent_conversation_metadata();
  parent_metadata->set_conversation_id("test_init_id");
  parent_metadata->set_conversation_title("test_init_title");

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  base::test::TestFuture<components_sharing_message::SharingMessage> future;
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _))
      .WillRepeatedly(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(std::move(message));
            return base::OnceClosure();
          });

  SendMessageAndWait(std::move(start_message));

  auto* new_tab = GetTabListInterface()->GetTab(initial_tab_count);
  ASSERT_TRUE(new_tab);
  ASSERT_OK(WaitForGlicInstanceBoundToTab(new_tab));

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testHandlesGetScreenshotRequestSuccessfully) {
  OptIn();

  // 1. Send an actuation request to open Glic and bind it to the tab.
  auto actuation_message = CreateTriggeringMessage(101);
  actuation_message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      actuation_future;
  SetupMessageSenderMock(&actuation_future);

  int initial_tab_count = GetTabListInterface()->GetTabCount();
  auto actuation_reply = SendMessageAndWait(std::move(actuation_message));
  ASSERT_TRUE(actuation_reply);
  ASSERT_TRUE(actuation_reply->has_glic_experimental_triggering());
  std::string context_id =
      actuation_reply->glic_experimental_triggering().context_id();

  // Verify that the instance is bound to the tab.
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), initial_tab_count + 1);
  auto* new_tab = GetTabListInterface()->GetTab(initial_tab_count);
  ASSERT_TRUE(new_tab);
  ASSERT_OK(WaitForGlicInstanceBoundToTab(new_tab));

  // Activate the background tab where Glic is opened so it is focused and
  // eligible for screenshot.
  GetTabListInterface()->ActivateTab(new_tab->GetHandle());
  ASSERT_EQ(GetTabListInterface()->GetActiveTab(), new_tab);

  // Run the JS side to subscribe.
  ExecuteJsTest();

  // Consume the actuation completion update.
  ASSERT_TRUE(actuation_future.Wait());
  auto [actuation_channel, actuation_response] = actuation_future.Take();
  EXPECT_EQ(actuation_channel.configuration(), "test_config");
  ASSERT_TRUE(actuation_response.has_glic_experimental_triggering());

  auto* glic_service = glic::GlicKeyedService::Get(GetProfile());
  ASSERT_TRUE(glic_service);
  glic::GlicInstance* instance = glic_service->GetInstanceForTab(new_tab);
  ASSERT_TRUE(instance);
  TestResult<actor::TaskId> task_id = CreateActorTask(instance);
  ASSERT_OK(task_id);
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  ASSERT_TRUE(actor_service);
  actor::ActorTask* task = actor_service->GetTask(task_id.value());
  ASSERT_TRUE(task);
  task->ObserveTabOnce(new_tab->GetHandle());

  // 2. Prepare and send the GetScreenshotRequest.
  // Use deterministic test vector (X9.62 uncompressed P-256 point) instead of
  // dynamically generating randomized key pairs.
  std::string public_key_str;
  ASSERT_TRUE(base::Base64Decode(
      "BFlvj1VrkwP8pxa1zSiJZzZ7yeMEO1DOPSbNw6XV8NK3Xo++7ql9NTcxNaciYM2eQ/"
      "G1ebnwrtRrHyMXEDhN5ck=",
      &public_key_str));
  std::string auth_secret_str(16, 'a');

  auto screenshot_message = CreateTriggeringMessage(102);
  screenshot_message.mutable_glic_experimental_triggering()->set_context_id(
      context_id);
  auto* screenshot_req =
      screenshot_message.mutable_glic_experimental_triggering()
          ->mutable_request()
          ->mutable_get_screenshot_request();
  screenshot_req->set_public_key(public_key_str);
  screenshot_req->set_auth_secret(auth_secret_str);
  screenshot_req->set_request_token("test_request_token");

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      screenshot_future;
  SetupMessageSenderMock(&screenshot_future);

  // Send the screenshot request.
  auto screenshot_reply = SendMessageAndWait(std::move(screenshot_message));
  ASSERT_TRUE(screenshot_reply);
  ASSERT_TRUE(screenshot_reply->has_glic_experimental_triggering());
  EXPECT_EQ(screenshot_reply->glic_experimental_triggering()
                .response()
                .task_update()
                .state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::STARTING);

  // 3. Verify the screenshot response contains the uploaded token.
  ASSERT_TRUE(screenshot_future.Wait());
  auto [server_channel, received_message] = screenshot_future.Take();
  EXPECT_EQ(server_channel.configuration(), "test_config");
  EXPECT_EQ(server_channel.p256dh(), kDefaultP256dh);
  EXPECT_EQ(server_channel.auth_secret(), kDefaultAuthSecret);
  EXPECT_FALSE(received_message.has_server_channel_configuration());
  ASSERT_TRUE(received_message.has_glic_experimental_triggering());
  ASSERT_TRUE(received_message.glic_experimental_triggering().has_response());

  const auto& response =
      received_message.glic_experimental_triggering().response();
  ASSERT_TRUE(response.has_screenshot_result());

  const auto& result = response.screenshot_result();
  EXPECT_EQ(result.status(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::ScreenshotResult::SUCCESS);
  EXPECT_EQ(result.file_token(), "mock_file_token_from_client");
  EXPECT_TRUE(result.error_message().empty());
  EXPECT_EQ(result.request_token(), "test_request_token");
}

class GlicExperimentalTriggeringOpenWindowTest
    : public GlicExperimentalTriggeringMessageHandlerBrowserTest {
 public:
  GlicExperimentalTriggeringOpenWindowTest() {
    open_window_feature_list_.InitAndEnableFeature(
        features::kGlicExperimentalTriggeringOpenWindowIfNone);
  }

 private:
  base::test::ScopedFeatureList open_window_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringOpenWindowTest,
                       OpensNewWindowWhenNoBrowserWindowAndFlagEnabled) {
  OptIn();

  auto mock_handler = std::make_unique<
      testing::NiceMock<MockGlicExperimentalTriggeringMessageHandler>>(
      GetProfile(), &mock_sharing_message_sender_);

  EXPECT_CALL(*mock_handler, GetBrowserWindow())
      .WillOnce(testing::Return(nullptr));

  auto message = CreateTriggeringMessage();
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->mutable_request()->mutable_trigger_actuation_request();

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;

  size_t initial_browser_count = GetAllBrowserWindowInterfaces().size();

  mock_handler->OnMessage(std::move(message), done_future.GetCallback());

  EXPECT_TRUE(done_future.Wait());
  auto response = done_future.Take();
  ASSERT_TRUE(response);
  EXPECT_TRUE(response->has_glic_experimental_triggering());
  EXPECT_EQ(
      response->glic_experimental_triggering().response().task_update().state(),
      components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringResponse::TaskUpdate::STARTING);
  EXPECT_FALSE(response->glic_experimental_triggering()
                   .response()
                   .task_update()
                   .has_data_type());

  // Verify that a new window was created.
  EXPECT_EQ(GetAllBrowserWindowInterfaces().size(), initial_browser_count + 1);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       AutoGeneratesContextIdWhenAbsent) {
  OptIn();

  components_sharing_message::SharingMessage message =
      CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()->clear_context_id();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());

  EXPECT_TRUE(done_future.Wait());
  auto response = done_future.Take();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->has_glic_experimental_triggering());

  std::string generated_context_id =
      response->glic_experimental_triggering().context_id();
  EXPECT_FALSE(generated_context_id.empty());
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  components_sharing_message::SharingMessage stop_message =
      CreateTriggeringMessage();
  stop_message.mutable_glic_experimental_triggering()->set_context_id(
      generated_context_id);
  stop_message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_stop_actuation_request()
      ->set_stop_reason("STOPPED_BY_USER");

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      stop_future;
  handler_->OnMessage(std::move(stop_message), stop_future.GetCallback());

  EXPECT_TRUE(stop_future.Wait());
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       AutoGeneratesContextIdWhenEmpty) {
  OptIn();

  components_sharing_message::SharingMessage message =
      CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()->set_context_id("");
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());

  EXPECT_TRUE(done_future.Wait());
  auto response = done_future.Take();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->has_glic_experimental_triggering());

  std::string generated_context_id =
      response->glic_experimental_triggering().context_id();
  EXPECT_FALSE(generated_context_id.empty());
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  components_sharing_message::SharingMessage stop_message =
      CreateTriggeringMessage();
  stop_message.mutable_glic_experimental_triggering()->set_context_id(
      generated_context_id);
  stop_message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_stop_actuation_request()
      ->set_stop_reason("STOPPED_BY_USER");

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      stop_future;
  handler_->OnMessage(std::move(stop_message), stop_future.GetCallback());

  EXPECT_TRUE(stop_future.Wait());
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}



IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testIncomingMessageResultMetricsForFailures) {
  {
    base::HistogramTester histogram_tester;
    auto message = CreateTriggeringMessage();
    message.mutable_glic_experimental_triggering()
        ->mutable_request()
        ->mutable_trigger_actuation_request();
    message.mutable_glic_experimental_triggering()->clear_task_metadata();
    SendMessageAndWait(std::move(message));
    histogram_tester.ExpectUniqueSample(
        "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
        GlicExperimentalTriggeringIncomingMessageResult::kMissingTaskMetadata,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    auto message = CreateTriggeringMessage();
    message.mutable_glic_experimental_triggering()
        ->set_glic_experimental_triggering_version(999);
    SendMessageAndWait(std::move(message));
    histogram_tester.ExpectUniqueSample(
        "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
        GlicExperimentalTriggeringIncomingMessageResult::
            kVersionMismatchOrUnavailable,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    auto message = CreateTriggeringMessage();
    message.clear_server_channel_configuration();
    SendMessageAndWait(std::move(message));
    histogram_tester.ExpectUniqueSample(
        "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
        GlicExperimentalTriggeringIncomingMessageResult::kMissingServerChannel,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    auto message = CreateTriggeringMessage();
    SendMessageAndWait(std::move(message));
    histogram_tester.ExpectUniqueSample(
        "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
        GlicExperimentalTriggeringIncomingMessageResult::kMissingPayload, 1);
  }

  {
    base::HistogramTester histogram_tester;
    auto message = CreateTriggeringMessage();
    message.mutable_glic_experimental_triggering()
        ->mutable_request()
        ->mutable_trigger_actuation_request();
    SendMessageAndWait(std::move(message));
    histogram_tester.ExpectUniqueSample(
        "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
        GlicExperimentalTriggeringIncomingMessageResult::kUserNotOptedIn, 1);
  }
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testDeferredRequestBackgroundTabPrepared) {
  // Enable the background triggering feature flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGlicBackgroundTriggering);

  OptIn();

  auto message = CreateTriggeringMessage(101);
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_context_id("test-context-id");
  triggering->mutable_request()->mutable_trigger_actuation_request();

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;

  // Send the message. Since kGlicBackgroundTriggering is enabled, it should
  // add the observer, save the request, and call
  // EnsureForegroundServiceStarted. It returns early and does NOT call
  // done_callback yet.
  handler_->OnMessage(std::move(message), done_future.GetCallback());

  // Verify that no response was sent yet (done_future is not ready).
  EXPECT_FALSE(done_future.IsReady());

  // Get the active tab to act as the prepared tab.
  tabs::TabInterface* prepared_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(prepared_tab);

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      sender_future;
  SetupMessageSenderMock(&sender_future);

  // Trigger OnBackgroundTabPrepared via ActorKeyedService. This should resume
  // the deferred request.
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  ASSERT_TRUE(actor_service);
  actor_service->NotifyBackgroundTabReady(prepared_tab, "test-context-id");

  // Verify that the request is completed.
  EXPECT_TRUE(done_future.Wait());
  auto response = done_future.Take();
  ASSERT_TRUE(response);
  EXPECT_EQ(
      response->glic_experimental_triggering().response().task_update().state(),
      components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringResponse::TaskUpdate::STARTING);

  // Verify that the updates handler was created.
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 1u);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalTriggeringMessageHandlerBrowserTest,
                       testDeferredRequestBackgroundSetupFailed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGlicBackgroundTriggering);

  OptIn();

  auto message = CreateTriggeringMessage(101);
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_context_id("test-context-id");
  triggering->mutable_request()->mutable_trigger_actuation_request();

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;

  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_FALSE(done_future.IsReady());

  // Trigger OnBackgroundSetupFailed via ActorKeyedService.
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  ASSERT_TRUE(actor_service);
  actor_service->NotifyBackgroundSetupFailed("test-context-id");

  // Verify that the request is completed with a failure response.
  EXPECT_TRUE(done_future.Wait());
  auto response = done_future.Take();
  ASSERT_TRUE(response);
  EXPECT_EQ(
      response->glic_experimental_triggering().response().task_update().state(),
      components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringResponse::TaskUpdate::FAILED);
  EXPECT_EQ(
      response->glic_experimental_triggering().response().task_update().data(),
      "Background setup failed.");

  // Handler map should be empty.
  EXPECT_EQ(handler_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}
#endif

}  // namespace glic

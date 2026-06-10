// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

#include <memory>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
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

components_sharing_message::SharingMessage CreateTriggeringMessage(
    int64_t sequence_number = kDefaultSequenceNumber,
    const std::string& server_config = kDefaultServerConfig) {
  components_sharing_message::SharingMessage message;
  message.mutable_server_channel_configuration()->set_configuration(
      server_config);
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
        {{features::kGlicExperimentalTriggering, {}}}, {});
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
            0);
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            101);
}

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
  EXPECT_EQ(message1.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            0);
  EXPECT_EQ(message1.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            42);

  auto message2 = future.Take();
  EXPECT_EQ(message2.glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            1);
  EXPECT_EQ(message2.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            42);
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
            0);
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
            0);
  EXPECT_EQ(received_message.glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            kDefaultSequenceNumber);
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
  EXPECT_EQ(start_response, nullptr);

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
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            0);
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            kDefaultSequenceNumber + 1);
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
  EXPECT_EQ(start_response, nullptr);

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
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            0);
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
  EXPECT_EQ(response, nullptr);

  // Verify that a new window was created.
  EXPECT_EQ(GetAllBrowserWindowInterfaces().size(), initial_browser_count + 1);
}

using TaskUpdate = components_sharing_message::GlicExperimentalTriggering::
    ExperimentalTriggeringResponse::TaskUpdate;

struct ExpectedTaskUpdate {
  TaskUpdate::State state;
  TaskUpdate::DataType data_type;
  std::string data;
};

struct TestScenarioParam {
  const char* test_name;
  components_sharing_message::SharingMessage message;
  std::optional<ExpectedTaskUpdate> expected_task_update;
  bool browser_window = true;
  int64_t expected_sender_sequence_number = 0;
  int64_t expected_last_seen_sequence_number = kDefaultSequenceNumber;
};

components_sharing_message::SharingMessage
BuildNoServerChannelExperimentalTriggeringMessage() {
  components_sharing_message::SharingMessage message =
      CreateTriggeringMessage();
  message.clear_server_channel_configuration();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();
  return message;
}

components_sharing_message::SharingMessage
BuildNoRequestPayloadExperimentalTriggeringMessage() {
  return CreateTriggeringMessage();
}

components_sharing_message::SharingMessage
BuildNoVersionNoBrowserWindowMessage() {
  components_sharing_message::SharingMessage message =
      CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->clear_glic_experimental_triggering_version();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();
  return message;
}

components_sharing_message::SharingMessage
BuildStopActuationNoMatchingUpdatesHandler() {
  auto message = CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_stop_actuation_request()
      ->set_stop_reason("STOPPED_BY_USER");

  return message;
}

components_sharing_message::SharingMessage BuildNewerVersionMessage() {
  components_sharing_message::SharingMessage message =
      CreateTriggeringMessage();
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_glic_experimental_triggering_version(2);
  triggering->mutable_request()->mutable_trigger_actuation_request();
  return message;
}

components_sharing_message::SharingMessage BuildSameVersionMessage() {
  components_sharing_message::SharingMessage message =
      CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();
  return message;
}

class GlicExperimentalTriggeringMessageHandlerResponseTest
    : public GlicExperimentalTriggeringMessageHandlerBrowserTest,
      public testing::WithParamInterface<TestScenarioParam> {};

IN_PROC_BROWSER_TEST_P(GlicExperimentalTriggeringMessageHandlerResponseTest,
                       ProducesExpectedResponse) {
  OptIn();

  auto mock_handler = std::make_unique<
      testing::NiceMock<MockGlicExperimentalTriggeringMessageHandler>>(
      GetProfile(), &mock_sharing_message_sender_);
  if (!GetParam().browser_window) {
    EXPECT_CALL(*mock_handler, GetBrowserWindow())
        .WillOnce(testing::Return(nullptr));
  }

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;

  mock_handler->OnMessage(std::move(GetParam().message),
                          done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  auto response = done_future.Take();
  if (!GetParam().expected_task_update) {
    EXPECT_EQ(response, nullptr);
    return;
  }

  ASSERT_TRUE(response);
  ASSERT_TRUE(response->has_glic_experimental_triggering());
  ASSERT_TRUE(response->glic_experimental_triggering().has_response());
  ASSERT_TRUE(
      response->glic_experimental_triggering().response().has_task_update());
  const auto& task_update =
      response->glic_experimental_triggering().response().task_update();
  EXPECT_EQ(task_update.state(), GetParam().expected_task_update->state);
  EXPECT_EQ(task_update.data_type(),
            GetParam().expected_task_update->data_type);
  EXPECT_EQ(task_update.data(), GetParam().expected_task_update->data);
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .sender_sequence_number(),
            GetParam().expected_sender_sequence_number);
  EXPECT_EQ(response->glic_experimental_triggering()
                .task_metadata()
                .last_seen_sequence_number(),
            GetParam().expected_last_seen_sequence_number);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicExperimentalTriggeringMessageHandlerResponseTest,
    testing::Values(
        TestScenarioParam{
            "NoServerChannelConfig",
            BuildNoServerChannelExperimentalTriggeringMessage(),
            ExpectedTaskUpdate{TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
                               "Received GlicExperimentalTriggering message "
                               "with no server configuration channel data."}},
        TestScenarioParam{
            "NoRequestPayload",
            BuildNoRequestPayloadExperimentalTriggeringMessage(),
            ExpectedTaskUpdate{TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
                               "Received GlicExperimentalTriggering "
                               "message with no request payload."}},
        TestScenarioParam{
            "NoBrowserWindow", BuildNoVersionNoBrowserWindowMessage(),
            ExpectedTaskUpdate{TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
                               "No browser window found for current profile"},
            /*browser_window=*/false},
        TestScenarioParam{
            "UnrecognizedStopActuation",
            BuildStopActuationNoMatchingUpdatesHandler(),
            ExpectedTaskUpdate{
                TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
                "Failed to stop task due to missing glic instance."}},
        TestScenarioParam{
            "NewerVersion", BuildNewerVersionMessage(),
            ExpectedTaskUpdate{TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
                               "Rejected: version mismatch or unavailable."}},
        TestScenarioParam{
            "SameVersionNoBrowserWindow", BuildSameVersionMessage(),
            ExpectedTaskUpdate{TaskUpdate::FAILED, TaskUpdate::ERROR_MESSAGE,
                               "No browser window found for current profile"},
            /*browser_window=*/false}),
    [](const testing::TestParamInfo<TestScenarioParam>& info) {
      return info.param.test_name;
    });

}  // namespace glic

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/browser_actuator/browser_actuator_message_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_actuator/proto/actuator_downstream_message.pb.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/test_support/mock_browser_actuator_service.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BrowserActuatorMessageHandlerTest : public testing::Test {
 public:
  BrowserActuatorMessageHandlerTest() {
    feature_list_.InitWithFeatures(
        {browser_actuator::kBrowserActuator,
         browser_actuator::kEnableBrowserActuatorForGlicExperimentalTriggering},
        {});
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    browser_actuator::BrowserActuatorServiceFactory::GetInstance()
        ->SetTestingFactory(
            profile_.get(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  browser_actuator::MockBrowserActuatorService>();
            }));
    mock_service_ = static_cast<browser_actuator::MockBrowserActuatorService*>(
        browser_actuator::BrowserActuatorServiceFactory::GetForProfile(
            profile_.get()));
    EXPECT_CALL(*mock_service_, IsInitialized())
        .WillRepeatedly(testing::Return(true));

    handler_ = std::make_unique<BrowserActuatorMessageHandler>(profile_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<browser_actuator::MockBrowserActuatorService> mock_service_;
  std::unique_ptr<BrowserActuatorMessageHandler> handler_;
};

TEST_F(BrowserActuatorMessageHandlerTest, HandlesInitialSharingMessage) {
  components_sharing_message::SharingMessage message;
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_context_id("test_context_123");
  triggering->set_glic_experimental_triggering_version(1);
  message.mutable_server_channel_configuration();
  triggering->mutable_request()->mutable_device_opt_in_request();

  EXPECT_CALL(*mock_service_, GetOrCreateSession("test_context_123"));

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

TEST_F(BrowserActuatorMessageHandlerTest, HandlesTriggerActuationMessage) {
  components_sharing_message::SharingMessage message;
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_context_id("test_context_123");
  triggering->set_glic_experimental_triggering_version(1);
  triggering->mutable_request()->mutable_trigger_actuation_request();

  EXPECT_CALL(*mock_service_, GetOrCreateSession("test_context_123"));

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

TEST_F(BrowserActuatorMessageHandlerTest, IgnoresUnsupportedMessage) {
  components_sharing_message::SharingMessage message;
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_context_id("test_context_123");
  triggering->set_glic_experimental_triggering_version(1);
  triggering->mutable_request()->mutable_stop_actuation_request();

  EXPECT_CALL(*mock_service_, GetOrCreateSession(testing::_)).Times(0);

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

TEST_F(BrowserActuatorMessageHandlerTest, IgnoresMessageWithNoRequest) {
  components_sharing_message::SharingMessage message;
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_context_id("test_context_123");
  triggering->set_glic_experimental_triggering_version(1);

  EXPECT_CALL(*mock_service_, GetOrCreateSession(testing::_)).Times(0);

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

TEST_F(BrowserActuatorMessageHandlerTest, HandlesActuatorDownstreamMessage) {
  components_sharing_message::SharingMessage message;
  browser_actuator::ActuatorDownstreamMessage* bundled =
      message.mutable_actuator_downstream_message();
  bundled->set_session_id("bundled_session_123");

  EXPECT_CALL(*mock_service_, GetOrCreateSession("bundled_session_123"));

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

}  // namespace

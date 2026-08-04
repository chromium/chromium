// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/browser_actuator/browser_actuator_message_handler.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_actuator/public/features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockGlicExperimentalTriggeringCoordinator
    : public glic::GlicExperimentalTriggeringCoordinator {
 public:
  explicit MockGlicExperimentalTriggeringCoordinator(Profile* profile)
      : glic::GlicExperimentalTriggeringCoordinator(profile) {}

  MOCK_METHOD(std::optional<glic::ExperimentalTriggeringResponse>,
              OnRequest,
              (const std::string& context_id,
               const glic::ExperimentalTriggeringRequest& request,
               glic::ScopedIncomingMessageResultLogger result_logger,
               glic::GlicExperimentalTriggeringUpdateCallback update_callback,
               tabs::TabInterface* prepared_tab),
              (override));
};

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
    glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_.get(),
                                                       /*create=*/true);
    auto coordinator =
        std::make_unique<MockGlicExperimentalTriggeringCoordinator>(
            profile_.get());
    mock_coordinator_ = coordinator.get();
    handler_ = std::make_unique<BrowserActuatorMessageHandler>(
        profile_.get(), std::move(coordinator));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<MockGlicExperimentalTriggeringCoordinator> mock_coordinator_;
  std::unique_ptr<BrowserActuatorMessageHandler> handler_;
};

TEST_F(BrowserActuatorMessageHandlerTest, HandlesInitialSharingMessage) {
  components_sharing_message::SharingMessage message;
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_context_id("test_context_123");
  triggering->set_glic_experimental_triggering_version(1);
  message.mutable_server_channel_configuration();
  triggering->mutable_request()->mutable_trigger_actuation_request();

  EXPECT_CALL(*mock_coordinator_,
              OnRequest(testing::Eq("test_context_123"), testing::_, testing::_,
                        testing::_, testing::_))
      .WillOnce(testing::Return(std::nullopt));

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
}

}  // namespace

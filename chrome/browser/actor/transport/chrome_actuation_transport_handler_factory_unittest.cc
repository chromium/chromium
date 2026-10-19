// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/transport/chrome_actuation_transport_handler_factory.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport_channel.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory_registry.h"
#include "components/browser_actuator/public/transport_session.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

namespace proto = ::chrome::aix::autobrowse::proto;

class FakeTransportSession : public browser_actuator::TransportSession {
 public:
  explicit FakeTransportSession(std::string_view session_id)
      : session_id_(session_id) {}
  ~FakeTransportSession() override = default;

  std::string_view GetSessionId() const override { return session_id_; }

  base::expected<void, browser_actuator::SendUpstreamMessageError>
  SendUpstreamMessage(browser_actuator::PayloadType payload_type,
                      const google::protobuf::MessageLite& message) override {
    sent_payload_types_.push_back(payload_type);
    proto::BrowserToServerPayload upstream;
    EXPECT_TRUE(upstream.ParseFromString(message.SerializeAsString()));
    sent_messages_.push_back(std::move(upstream));
    return {};
  }

  void OnMessage(browser_actuator::PayloadType payload_type,
                 const google::protobuf::MessageLite& message) override {}

  const std::vector<browser_actuator::PayloadType>& sent_payload_types() const {
    return sent_payload_types_;
  }

  const std::vector<proto::BrowserToServerPayload>& sent_messages() const {
    return sent_messages_;
  }

 private:
  std::string session_id_;
  std::vector<browser_actuator::PayloadType> sent_payload_types_;
  std::vector<proto::BrowserToServerPayload> sent_messages_;
};

class ChromeActuationTransportHandlerFactoryTest : public testing::Test {
 public:
  ChromeActuationTransportHandlerFactoryTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ChromeActuationTransportHandlerFactoryTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("profile");
    actor_keyed_service_ = ActorKeyedService::Get(profile_);
    ASSERT_TRUE(actor_keyed_service_);
    factory_ = std::make_unique<ChromeActuationTransportHandlerFactory>(
        actor_keyed_service_);
  }

  void TearDown() override {
    actor_keyed_service_->Shutdown();
    factory_.reset();
    actor_keyed_service_ = nullptr;
    profile_ = nullptr;
  }

 protected:
  base::test::ScopedFeatureList feature_list_{
      browser_actuator::kBrowserActuator};
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<ActorKeyedService> actor_keyed_service_ = nullptr;
  std::unique_ptr<ChromeActuationTransportHandlerFactory> factory_;
};

TEST_F(ChromeActuationTransportHandlerFactoryTest,
       ReportsActuationFactoryIdAndSupportedPayloadTypes) {
  EXPECT_EQ(factory_->GetFactoryId(), browser_actuator::FactoryId::kActuation);
  std::vector<browser_actuator::PayloadType> expected_types = {
      browser_actuator::PayloadType::kActuation};
  EXPECT_EQ(factory_->GetSupportedPayloadTypes(), expected_types);
}

TEST_F(ChromeActuationTransportHandlerFactoryTest,
       OnNewSessionCreatesHandlerThatRoutesStartTaskAndStopsOnDestruction) {
  FakeTransportSession session("session_factory_1");
  std::unique_ptr<browser_actuator::TransportHandler> handler =
      factory_->OnNewSession(&session);
  ASSERT_TRUE(handler);

  proto::ServerToBrowserPayload downstream;
  downstream.set_request_id("req_factory_start");
  downstream.mutable_start_task()->add_target_surfaces()->set_tab_id(15);

  handler->OnMessage(browser_actuator::PayloadType::kActuation,
                     downstream.SerializeAsString());

  ASSERT_EQ(session.sent_messages().size(), 1u);
  EXPECT_EQ(session.sent_payload_types()[0],
            browser_actuator::PayloadType::kActuation);
  const auto& upstream = session.sent_messages()[0];
  EXPECT_EQ(upstream.request_id(), "req_factory_start");
  ASSERT_TRUE(upstream.has_start_task());
  EXPECT_EQ(upstream.start_task().status(),
            proto::StartTaskResponse::STATUS_OK);

  int task_id_val = 0;
  ASSERT_TRUE(base::StringToInt(upstream.task_id(), &task_id_val));
  TaskId task_id(task_id_val);
  ActorTask* task = actor_keyed_service_->GetTask(task_id);
  ASSERT_TRUE(task);
  EXPECT_EQ(task->source_info().type, TaskSourceInfo::Client::kBrowserActuator);
  EXPECT_FALSE(task->source_info().id.has_value());
  EXPECT_NE(&task->policy_checker(), GetNullEnterprisePolicyChecker());

  handler.reset();
  EXPECT_FALSE(actor_keyed_service_->GetTask(task_id));
}

TEST_F(ChromeActuationTransportHandlerFactoryTest,
       RegisteredWithBrowserActuatorServiceWhenEnabled) {
  browser_actuator::BrowserActuatorService* actuator_service =
      browser_actuator::BrowserActuatorServiceFactory::GetForProfile(profile_);
  ASSERT_TRUE(actuator_service);
  ASSERT_TRUE(actuator_service->GetChannel());
  browser_actuator::TransportHandlerFactoryRegistry* registry =
      actuator_service->GetChannel()->GetHandlerFactoryRegistry();
  ASSERT_TRUE(registry);

  std::vector<browser_actuator::TransportHandlerFactory*> factories =
      registry->GetFactories(browser_actuator::PayloadType::kActuation);
  ASSERT_EQ(factories.size(), 1u);
  EXPECT_EQ(factories[0]->GetFactoryId(),
            browser_actuator::FactoryId::kActuation);

  browser_actuator::TransportSession* session =
      actuator_service->GetOrCreateSession("enabled_session_1");
  ASSERT_TRUE(session);
  proto::ServerToBrowserPayload downstream;
  downstream.set_request_id("req_enabled_start");
  downstream.mutable_start_task()->add_target_surfaces()->set_tab_id(15);
  session->OnMessage(browser_actuator::PayloadType::kActuation, downstream);
  EXPECT_EQ(actor_keyed_service_->GetActiveTasks().size(), 1u);

  actor_keyed_service_->Shutdown();
  EXPECT_TRUE(actor_keyed_service_->GetActiveTasks().empty());
  EXPECT_TRUE(registry->GetFactories(browser_actuator::PayloadType::kActuation)
                  .empty());
}

}  // namespace
}  // namespace actor

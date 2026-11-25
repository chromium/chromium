// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/stream_transport/transport_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_actuator/internal/proto/task_ingress.pb.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/proto/auto_password_change.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_channel.h"
#include "components/stream_transport/public/transport_handler_factory_registry.h"
#include "components/stream_transport/public/transport_service.h"
#include "components/stream_transport/public/transport_session.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace browser_actuator {
namespace {

using ::testing::Field;
using ::testing::StrictMock;

class StubTransportSession : public stream_transport::TransportSession {
 public:
  StubTransportSession() = default;
  ~StubTransportSession() override = default;

  std::string_view GetSessionId() const override { return "stub-session"; }
  base::expected<void, stream_transport::SendUpstreamMessageError>
  SendUpstreamMessage(stream_transport::PayloadType payload_type,
                      const google::protobuf::MessageLite& message) override {
    return base::ok();
  }
  void OnMessage(stream_transport::PayloadType payload_type,
                 std::string_view serialized_payload) override {}
  void Close() override {}
};

class RecordingTransportService : public stream_transport::TransportService {
 public:
  RecordingTransportService() = default;
  ~RecordingTransportService() override = default;

  stream_transport::TransportChannel* GetChannel() override { return nullptr; }
  stream_transport::TransportSession* CreateSession(
      std::string_view session_id,
      stream_transport::InitIngressSource ingress_source) override {
    return nullptr;
  }
  stream_transport::TransportSession* GetSession(
      std::string_view session_id) override {
    return nullptr;
  }
  stream_transport::SessionStreamRecorderFactory* GetStreamRecorderFactory()
      override {
    return nullptr;
  }
  stream_transport::TransportSession* StartClientSession(
      std::string_view session_id,
      stream_transport::PayloadType payload_type,
      const google::protobuf::MessageLite& message,
      StartClientSessionCallback callback) override {
    ++start_count_;
    last_serialized_message_ = message.SerializeAsString();
    pending_callback_ = std::move(callback);
    return &pending_session_;
  }

  int start_count() const { return start_count_; }
  const std::string& last_serialized_message() const {
    return last_serialized_message_;
  }

 private:
  int start_count_ = 0;
  std::string last_serialized_message_;
  StartClientSessionCallback pending_callback_;
  StubTransportSession pending_session_;
};

class MockObserver : public BrowserActuatorTaskClient::Observer {
 public:
  MOCK_METHOD(void, OnTaskStarted, (), (override));
  MOCK_METHOD(void, OnWorklogUpdate, (std::string_view), (override));
  MOCK_METHOD(void,
              OnTaskCompleted,
              (const BrowserActuatorTaskClient::Result&),
              (override));
};

}  // namespace

class BrowserActuatorServiceFactoryTest : public testing::Test {
 protected:
  BrowserActuatorServiceFactoryTest() = default;
  ~BrowserActuatorServiceFactoryTest() override = default;

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserActuatorServiceFactoryTest, UsesRealService) {
  feature_list_.InitAndEnableFeature(kBrowserActuator);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);
  EXPECT_TRUE(service->IsInitialized());
}

TEST_F(BrowserActuatorServiceFactoryTest, ReturnsNullIfFeatureDisabled) {
  feature_list_.InitAndDisableFeature(kBrowserActuator);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(profile.get());
  EXPECT_EQ(nullptr, service);
}

TEST_F(BrowserActuatorServiceFactoryTest, ReturnsNullInIncognito) {
  feature_list_.InitAndEnableFeature(kBrowserActuator);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  Profile* otr_profile = profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(otr_profile);
  EXPECT_EQ(nullptr, service);
}

TEST_F(BrowserActuatorServiceFactoryTest, ReturnsNullInGuest) {
  feature_list_.InitAndEnableFeature(kBrowserActuator);
  std::unique_ptr<TestingProfile> profile =
      TestingProfile::Builder().SetGuestSession().Build();

  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(profile.get());
  EXPECT_EQ(nullptr, service);
}

TEST_F(BrowserActuatorServiceFactoryTest, CreatesTaskClientWhenIngressEnabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuator, kBrowserActuatorTaskIngress},
      /*disabled_features=*/{});
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(profile.get());
  ASSERT_NE(nullptr, service);
  EXPECT_TRUE(service->IsInitialized());

  std::unique_ptr<BrowserActuatorTaskClient> task_client =
      service->CreateTaskClient();
  EXPECT_NE(nullptr, task_client);

  stream_transport::TransportService* transport_service =
      stream_transport::TransportServiceFactory::GetForProfile(profile.get());
  ASSERT_NE(nullptr, transport_service);
  ASSERT_NE(nullptr, transport_service->GetChannel());
  stream_transport::TransportHandlerFactoryRegistry* registry =
      transport_service->GetChannel()->GetHandlerFactoryRegistry();
  ASSERT_NE(nullptr, registry);
  EXPECT_EQ(registry->GetFactories(stream_transport::PayloadType::kTaskIngress)
                .size(),
            1u);

  service->Shutdown();
  EXPECT_TRUE(
      registry->GetFactories(stream_transport::PayloadType::kTaskIngress)
          .empty());
  EXPECT_EQ(nullptr, service->CreateTaskClient());
}

TEST_F(BrowserActuatorServiceFactoryTest,
       TaskClientSurvivesProfileDestructionWithoutDanglingPtr) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuator, kBrowserActuatorTaskIngress},
      /*disabled_features=*/{});

  // Declare `task_client` before `profile` so `profile.reset()` runs full
  // two-phase `KeyedService` shutdown and destroys `TransportServiceImpl` while
  // `task_client` is still alive.
  std::unique_ptr<BrowserActuatorTaskClient> task_client;
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(profile.get());
  ASSERT_NE(nullptr, service);
  task_client = service->CreateTaskClient();
  ASSERT_NE(nullptr, task_client);

  profile.reset();
  task_client->Cancel();
  task_client.reset();
}

TEST_F(BrowserActuatorServiceFactoryTest,
       StartTaskPopulatesSyncDeviceGuidAndFailsWhenUnavailable) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuator, kBrowserActuatorTaskIngress},
      /*disabled_features=*/{});

  TestingProfile::Builder builder;
  builder.AddTestingFactory(
      stream_transport::TransportServiceFactory::GetInstance(),
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<RecordingTransportService>();
          }));
  builder.AddTestingFactory(
      DeviceInfoSyncServiceFactory::GetInstance(),
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<syncer::FakeDeviceInfoSyncService>();
          }));
  std::unique_ptr<TestingProfile> profile = builder.Build();

  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(profile.get());
  ASSERT_NE(nullptr, service);

  auto* recording_transport = static_cast<RecordingTransportService*>(
      stream_transport::TransportServiceFactory::GetForProfile(profile.get()));
  ASSERT_NE(nullptr, recording_transport);
  auto* fake_sync = static_cast<syncer::FakeDeviceInfoSyncService*>(
      DeviceInfoSyncServiceFactory::GetForProfile(profile.get()));
  ASSERT_NE(nullptr, fake_sync);

  const syncer::DeviceInfo* local_info =
      fake_sync->GetLocalDeviceInfoProvider()->GetLocalDeviceInfo();
  ASSERT_NE(nullptr, local_info);
  const std::string expected_guid = local_info->guid();
  ASSERT_FALSE(expected_guid.empty());

  StrictMock<MockObserver> observer;
  std::unique_ptr<BrowserActuatorTaskClient> client_with_guid =
      service->CreateTaskClient();
  ASSERT_NE(nullptr, client_with_guid);

  AutoPasswordChangeTaskPayload payload;
  payload.set_domain("example.com");
  client_with_guid->StartTask(
      BrowserActuatorTaskClient::UseCase::kAutoPasswordChange, payload,
      &observer);
  EXPECT_EQ(recording_transport->start_count(), 1);

  TaskIngressUpstreamPayload upstream;
  ASSERT_TRUE(
      upstream.ParseFromString(recording_transport->last_serialized_message()));
  EXPECT_EQ(upstream.start_task().client_id(), expected_guid);
  client_with_guid.reset();

  // Clearing local `DeviceInfo` readiness causes `StartTask()` to fail
  // asynchronously without starting a transport session.
  fake_sync->GetLocalDeviceInfoProvider()->SetReady(false);
  std::unique_ptr<BrowserActuatorTaskClient> client_without_guid =
      service->CreateTaskClient();
  ASSERT_NE(nullptr, client_without_guid);

  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnTaskCompleted(
                  Field(&BrowserActuatorTaskClient::Result::status,
                        BrowserActuatorTaskClient::CompletionStatus::kFailed)))
      .WillOnce([&run_loop](const BrowserActuatorTaskClient::Result&) {
        run_loop.Quit();
      });
  client_without_guid->StartTask(
      BrowserActuatorTaskClient::UseCase::kAutoPasswordChange, payload,
      &observer);
  EXPECT_EQ(recording_transport->start_count(), 1);
  run_loop.Run();
}

TEST_F(BrowserActuatorServiceFactoryTest, NoTaskClientWhenIngressDisabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuator},
      /*disabled_features=*/{kBrowserActuatorTaskIngress});
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(profile.get());
  ASSERT_NE(nullptr, service);
  EXPECT_TRUE(service->IsInitialized());

  std::unique_ptr<BrowserActuatorTaskClient> task_client =
      service->CreateTaskClient();
  EXPECT_EQ(nullptr, task_client);

  stream_transport::TransportService* transport_service =
      stream_transport::TransportServiceFactory::GetForProfile(profile.get());
  ASSERT_NE(nullptr, transport_service);
  ASSERT_NE(nullptr, transport_service->GetChannel());
  EXPECT_TRUE(transport_service->GetChannel()
                  ->GetHandlerFactoryRegistry()
                  ->GetFactories(stream_transport::PayloadType::kTaskIngress)
                  .empty());
}

}  // namespace browser_actuator

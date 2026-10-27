// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui_mojo_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/browser_actuator/internals/browser_actuator_internals.mojom.h"
#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui.h"
#include "chrome/browser/browser_actuator/transport/transport_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/session_stream_recorder.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/transport_service.h"
#include "components/browser_actuator/public/transport/transport_session.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

class MockBrowserActuatorInternalsPage
    : public browser_actuator_internals::mojom::BrowserActuatorInternalsPage {
 public:
  MockBrowserActuatorInternalsPage() = default;
  ~MockBrowserActuatorInternalsPage() override = default;

  mojo::PendingRemote<
      browser_actuator_internals::mojom::BrowserActuatorInternalsPage>
  BindAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<
      browser_actuator_internals::mojom::BrowserActuatorInternalsPage>
      receiver_{this};
};

}  // namespace

class BrowserActuatorInternalsUIMojoImplTest : public testing::Test {
 public:
  BrowserActuatorInternalsUIMojoImplTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kBrowserActuator, kBrowserActuatorInternals},
        /*disabled_features=*/{});
    TestingProfile::Builder builder;
    builder.SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    profile_ = builder.Build();
    mojo_impl_ = std::make_unique<BrowserActuatorInternalsUIMojoImpl>(
        ui_remote_.BindNewPipeAndPassReceiver(), page_mock_.BindAndPassRemote(),
        profile_.get());
  }
  ~BrowserActuatorInternalsUIMojoImplTest() override = default;

  void TearDown() override {
    mojo_impl_.reset();
    profile_.reset();
  }

  void RecordStartSessionCommand(std::string_view session_id) {
    transport::TransportService* transport_service =
        transport::TransportServiceFactory::GetForProfile(profile_.get());
    ASSERT_NE(transport_service, nullptr);

    transport::TransportSession* session =
        transport_service->CreateSession(session_id);
    ASSERT_NE(session, nullptr);

    ControlCommand command;
    command.mutable_start_session();
    session->OnMessage(transport::PayloadType::kControl,
                       command.SerializeAsString());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  MockBrowserActuatorInternalsPage page_mock_;
  mojo::Remote<browser_actuator_internals::mojom::BrowserActuatorInternalsUI>
      ui_remote_;
  std::unique_ptr<BrowserActuatorInternalsUIMojoImpl> mojo_impl_;
};

TEST_F(BrowserActuatorInternalsUIMojoImplTest, BindsMojoEndpoints) {
  EXPECT_TRUE(ui_remote_.is_bound());
  ui_remote_.FlushForTesting();
  EXPECT_TRUE(ui_remote_.is_connected());
}

TEST_F(BrowserActuatorInternalsUIMojoImplTest,
       GetSessionHistoryReturnsEmptyWithNoSessions) {
  base::test::TestFuture<std::vector<transport::SessionSnapshot>> future;
  ui_remote_->GetSessionHistory(
      future.GetCallback<const std::vector<transport::SessionSnapshot>&>());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(BrowserActuatorInternalsUIMojoImplTest,
       GetSessionHistoryReturnsEmptyWhenInternalsDisabled) {
  base::test::ScopedFeatureList disabled_internals;
  disabled_internals.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuator},
      /*disabled_features=*/{kBrowserActuatorInternals});
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  mojo::Remote<browser_actuator_internals::mojom::BrowserActuatorInternalsUI>
      remote;
  MockBrowserActuatorInternalsPage page;
  auto impl = std::make_unique<BrowserActuatorInternalsUIMojoImpl>(
      remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      profile.get());

  base::test::TestFuture<std::vector<transport::SessionSnapshot>> future;
  remote->GetSessionHistory(
      future.GetCallback<const std::vector<transport::SessionSnapshot>&>());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(BrowserActuatorInternalsUIMojoImplTest,
       GetSessionHistoryReturnsRecordedSession) {
  RecordStartSessionCommand("test_session_1");

  base::test::TestFuture<std::vector<transport::SessionSnapshot>> future;
  ui_remote_->GetSessionHistory(
      future.GetCallback<const std::vector<transport::SessionSnapshot>&>());
  const std::vector<transport::SessionSnapshot>& sessions = future.Get();

  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].session_id, "test_session_1");
  EXPECT_FALSE(sessions[0].start_wall_time.is_null());
  EXPECT_FALSE(sessions[0].end_wall_time.has_value());
  EXPECT_EQ(sessions[0].total_downstream_messages, 1u);
  EXPECT_EQ(sessions[0].total_upstream_messages, 1u);
  EXPECT_EQ(sessions[0].total_events, 2u);
  ASSERT_EQ(sessions[0].events.size(), 2u);
  // ControlTransportHandler responds synchronously with StartSessionAck during
  // OnMessage dispatch, recording the upstream ack before the downstream entry.
  EXPECT_FALSE(sessions[0].events[0].timestamp.is_null());
  EXPECT_FALSE(sessions[0].events[0].is_downstream);
  ASSERT_EQ(sessions[0].events[0].payload_types.size(), 1u);
  EXPECT_EQ(sessions[0].events[0].payload_types[0], "Control");
  EXPECT_FALSE(sessions[0].events[0].message_truncated);
  EXPECT_FALSE(sessions[0].events[0].message.empty());
  EXPECT_FALSE(sessions[0].events[1].timestamp.is_null());
  EXPECT_TRUE(sessions[0].events[1].is_downstream);
  ASSERT_EQ(sessions[0].events[1].payload_types.size(), 1u);
  EXPECT_EQ(sessions[0].events[1].payload_types[0], "Control");
  EXPECT_FALSE(sessions[0].events[1].message_truncated);
  EXPECT_FALSE(sessions[0].events[1].message.empty());
}

TEST_F(BrowserActuatorInternalsUIMojoImplTest,
       GetSessionHistoryReturnsEmptyWhenProfileIsNull) {
  mojo::Remote<browser_actuator_internals::mojom::BrowserActuatorInternalsUI>
      remote;
  MockBrowserActuatorInternalsPage page;
  auto impl = std::make_unique<BrowserActuatorInternalsUIMojoImpl>(
      remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      /*profile=*/nullptr);

  base::test::TestFuture<std::vector<transport::SessionSnapshot>> future;
  remote->GetSessionHistory(
      future.GetCallback<const std::vector<transport::SessionSnapshot>&>());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(BrowserActuatorInternalsUIMojoImplTest,
       ExportSessionHistoryReturnsRecordedSessionsJson) {
  RecordStartSessionCommand("test_session_1");
  RecordStartSessionCommand("test_session_2");

  base::test::TestFuture<std::string> future;
  ui_remote_->ExportSessionHistory({"test_session_1"},
                                   future.GetCallback<const std::string&>());
  const std::string& json = future.Get();
  EXPECT_FALSE(json.empty());

  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed.has_value());
  const base::ListValue* sessions = parsed->FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 1u);
  const base::DictValue* session_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(session_dict, nullptr);
  const std::string* session_id = session_dict->FindString("session_id");
  ASSERT_NE(session_id, nullptr);
  EXPECT_EQ(*session_id, "test_session_1");
  const base::ListValue* events = session_dict->FindList("events");
  ASSERT_NE(events, nullptr);
  EXPECT_EQ(events->size(), 2u);
}

TEST_F(BrowserActuatorInternalsUIMojoImplTest,
       ExportSessionHistoryReturnsEmptyWhenInternalsDisabled) {
  base::test::ScopedFeatureList disabled_internals;
  disabled_internals.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuator},
      /*disabled_features=*/{kBrowserActuatorInternals});
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  mojo::Remote<browser_actuator_internals::mojom::BrowserActuatorInternalsUI>
      remote;
  MockBrowserActuatorInternalsPage page;
  auto impl = std::make_unique<BrowserActuatorInternalsUIMojoImpl>(
      remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      profile.get());

  base::test::TestFuture<std::string> future;
  remote->ExportSessionHistory({"test_session_1"},
                               future.GetCallback<const std::string&>());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(BrowserActuatorInternalsUIMojoImplTest,
       ExportSessionHistoryReturnsEmptyWhenProfileIsNull) {
  mojo::Remote<browser_actuator_internals::mojom::BrowserActuatorInternalsUI>
      remote;
  MockBrowserActuatorInternalsPage page;
  auto impl = std::make_unique<BrowserActuatorInternalsUIMojoImpl>(
      remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      /*profile=*/nullptr);

  base::test::TestFuture<std::string> future;
  remote->ExportSessionHistory({"test_session_1"},
                               future.GetCallback<const std::string&>());
  EXPECT_TRUE(future.Get().empty());
}

TEST(BrowserActuatorInternalsUIConfigTest, IsWebUIEnabled) {
  base::test::ScopedFeatureList feature_list;
  BrowserActuatorInternalsUIConfig config;

  // Both features enabled -> WebUI is enabled.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuator, kBrowserActuatorInternals},
      /*disabled_features=*/{});
  EXPECT_TRUE(config.IsWebUIEnabled(/*browser_context=*/nullptr));

  // Only kBrowserActuator enabled -> WebUI is disabled.
  feature_list.Reset();
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuator},
      /*disabled_features=*/{kBrowserActuatorInternals});
  EXPECT_FALSE(config.IsWebUIEnabled(/*browser_context=*/nullptr));

  // Only kBrowserActuatorInternals enabled -> WebUI is disabled.
  feature_list.Reset();
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuatorInternals},
      /*disabled_features=*/{kBrowserActuator});
  EXPECT_FALSE(config.IsWebUIEnabled(/*browser_context=*/nullptr));

  // Both disabled -> WebUI is disabled.
  feature_list.Reset();
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kBrowserActuator, kBrowserActuatorInternals});
  EXPECT_FALSE(config.IsWebUIEnabled(/*browser_context=*/nullptr));
}

}  // namespace browser_actuator

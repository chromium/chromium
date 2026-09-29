// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui_mojo_impl.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"
#include "chrome/browser/browser_actuator/internals/browser_actuator_internals.mojom.h"
#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/session_stream_recorder.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport_session.h"
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
  base::test::TestFuture<std::vector<SessionSnapshot>> future;
  ui_remote_->GetSessionHistory(
      future.GetCallback<const std::vector<SessionSnapshot>&>());
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

  base::test::TestFuture<std::vector<SessionSnapshot>> future;
  remote->GetSessionHistory(
      future.GetCallback<const std::vector<SessionSnapshot>&>());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(BrowserActuatorInternalsUIMojoImplTest,
       GetSessionHistoryReturnsRecordedSession) {
  BrowserActuatorService* service =
      BrowserActuatorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(service, nullptr);

  TransportSession* session = service->GetOrCreateSession("test_session_1");
  ASSERT_NE(session, nullptr);

  ControlCommand command;
  command.mutable_start_session();
  session->OnMessage(PayloadType::kControl, command);

  base::test::TestFuture<std::vector<SessionSnapshot>> future;
  ui_remote_->GetSessionHistory(
      future.GetCallback<const std::vector<SessionSnapshot>&>());
  const std::vector<SessionSnapshot>& sessions = future.Get();

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

  base::test::TestFuture<std::vector<SessionSnapshot>> future;
  remote->GetSessionHistory(
      future.GetCallback<const std::vector<SessionSnapshot>&>());
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

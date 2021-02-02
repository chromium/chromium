// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromebox_for_meetings/logger/cfm_logger_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/components/chromebox_for_meetings/features/features.h"
#include "chromeos/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_context.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_logger.mojom-shared.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_logger.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace cfm {
namespace {

class FakeCfmLoggerServiceDelegate : public CfmLoggerService::Delegate {
 public:
  void Init() override { init_count_++; }

  void Reset() override { reset_count_++; }

  void Enqueue(const std::string& record,
               mojom::EnqueuePriority priority,
               mojom::MeetDevicesLogger::EnqueueCallback callback) override {
    enqueue_count_++;
    enqueue_record_ = record;
    enqueue_priority_ = std::move(priority);
    std::move(callback).Run(mojom::LoggerState::kReadyForRequests);
  }

  int init_count_ = 0;
  int reset_count_ = 0;
  int enqueue_count_ = 0;
  std::string enqueue_record_;
  mojom::EnqueuePriority enqueue_priority_;
};

class CfmLoggerServiceTest : public testing::Test {
 public:
  CfmLoggerServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::cfm::features::kMojoServices,
         chromeos::cfm::features::kCloudLogger},
        {});
  }
  CfmLoggerServiceTest(const CfmLoggerServiceTest&) = delete;
  CfmLoggerServiceTest& operator=(const CfmLoggerServiceTest&) = delete;
  ~CfmLoggerServiceTest() override = default;

  void SetUp() override {
    CfmHotlineClient::InitializeFake();
    ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    CfmLoggerService::InitializeForTesting(&logger_delegate_);
  }

  void TearDown() override {
    CfmLoggerService::Shutdown();
    CfmHotlineClient::Shutdown();
  }

  FakeCfmHotlineClient* GetClient() {
    return static_cast<FakeCfmHotlineClient*>(CfmHotlineClient::Get());
  }

  mojo::Remote<mojom::MeetDevicesLogger> GetLoggerRemote() {
    base::RunLoop run_loop;

    auto* interface_name = mojom::MeetDevicesLogger::Name_;

    // Fake out CfmServiceContext
    FakeCfmServiceContext context;
    mojo::Receiver<mojom::CfmServiceContext> context_receiver(&context);
    fake_service_connection_.SetCallback(base::BindLambdaForTesting(
        [&](mojo::PendingReceiver<mojom::CfmServiceContext> pending_receiver,
            bool success) {
          ASSERT_TRUE(success);
          context_receiver.Bind(std::move(pending_receiver));
        }));

    mojo::Remote<mojom::CfmServiceAdaptor> logger_adaptor;
    context.SetFakeProvideAdaptorCallback(base::BindLambdaForTesting(
        [&](const std::string& service_id,
            mojo::PendingRemote<mojom::CfmServiceAdaptor> adaptor_remote,
            mojom::CfmServiceContext::ProvideAdaptorCallback callback) {
          ASSERT_EQ(interface_name, service_id);
          logger_adaptor.Bind(std::move(adaptor_remote));
          std::move(callback).Run(true);
        }));

    EXPECT_TRUE(GetClient()->FakeEmitSignal(interface_name));
    run_loop.RunUntilIdle();

    EXPECT_TRUE(context_receiver.is_bound());
    EXPECT_TRUE(logger_adaptor.is_connected());

    mojo::Remote<mojom::MeetDevicesLogger> logger_remote;
    logger_adaptor->OnBindService(
        logger_remote.BindNewPipeAndPassReceiver().PassPipe());
    EXPECT_TRUE(logger_remote.is_connected());

    return logger_remote;
  }

  void DisableLoggerFeature() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {chromeos::cfm::features::kMojoServices},
        {chromeos::cfm::features::kCloudLogger});
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeServiceConnectionImpl fake_service_connection_;
  FakeCfmLoggerServiceDelegate logger_delegate_;
};

// This test ensures that the CfmBrowserService is discoverable by its mojom
// name by sending a signal received by CfmHotlineClient.
TEST_F(CfmLoggerServiceTest, CfmLoggerServiceAvailable) {
  ASSERT_TRUE(GetClient()->FakeEmitSignal(mojom::MeetDevicesLogger::Name_));
}

// This test ensures that the CfmBrowserService discoverability is correctly
// controlled by the feature flag
TEST_F(CfmLoggerServiceTest, CfmLoggerServiceNotAvailable) {
  DisableLoggerFeature();
  auto* interface_name = mojom::MeetDevicesLogger::Name_;
  bool test_success = GetClient()->FakeEmitSignal(interface_name);
  ASSERT_FALSE(test_success);
}

// This test ensures that the CfmBrowserService correctly registers itself for
// discovery by the cfm mojom binder daemon and correctly returns a working
// mojom remote.
TEST_F(CfmLoggerServiceTest, GetLoggerRemote) {
  base::RunLoop run_loop;
  ASSERT_TRUE(GetLoggerRemote().is_connected());
  ASSERT_EQ(logger_delegate_.init_count_, 1);
  run_loop.RunUntilIdle();
  EXPECT_EQ(logger_delegate_.reset_count_, 1);
}

}  // namespace
}  // namespace cfm
}  // namespace chromeos

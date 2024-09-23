// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/logger/cfm_logger_service.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/chromebox_for_meetings/features.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_context.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_logger.mojom-shared.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_logger.mojom.h"
#include "components/reporting/util/status.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::cfm {
namespace {

// TODO(https://crbug.com/1403174): Remove when namespace of mojoms for CfM are
// migarted to ash.
namespace mojom = ::chromeos::cfm::mojom;

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
    std::move(callback).Run(mojom::LoggerStatus::New(
        mojom::LoggerErrorCode::kOk, "Debug Message."));
  }

  int init_count_ = 0;
  int reset_count_ = 0;
  int enqueue_count_ = 0;
  std::string enqueue_record_;
  mojom::EnqueuePriority enqueue_priority_;
};

class FakeLoggerStateObserver : public mojom::LoggerStateObserver {
 public:
  using OnNotifyStateCallback =
      base::RepeatingCallback<void(mojom::LoggerState)>;

  void OnNotifyState(mojom::LoggerState state) override {
    if (!on_notify_state_callback_.is_null()) {
      on_notify_state_callback_.Run(std::move(state));
    }
    on_notify_state_count_++;
  }

  int on_notify_state_count_ = 0;
  OnNotifyStateCallback on_notify_state_callback_;
  mojo::Receiver<mojom::LoggerStateObserver> receiver_{this};
};

class CfmLoggerServiceTest : public testing::Test {
 public:
  CfmLoggerServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kMojoServices, features::kCloudLogger}, {});
  }
  CfmLoggerServiceTest(const CfmLoggerServiceTest&) = delete;
  CfmLoggerServiceTest& operator=(const CfmLoggerServiceTest&) = delete;
  ~CfmLoggerServiceTest() override = default;

  void SetUp() override {
    CfmHotlineClient::InitializeFake();
    chromeos::cfm::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
  }

  void TearDown() override {
    CfmLoggerService::Shutdown();
    CfmHotlineClient::Shutdown();
  }

  FakeCfmHotlineClient* GetClient() {
    return static_cast<FakeCfmHotlineClient*>(CfmHotlineClient::Get());
  }

  mojo::Remote<mojom::MeetDevicesLogger> GetLoggerRemote() {
    if (!CfmLoggerService::IsInitialized()) {
      CfmLoggerService::InitializeForTesting(&logger_delegate_);
    }

    base::RunLoop run_loop;

    auto* interface_name = mojom::MeetDevicesLogger::Name_;

    // Fake out CfmServiceContext
    chromeos::cfm::FakeCfmServiceContext context;
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
    scoped_feature_list_.InitWithFeatures({features::kMojoServices},
                                          {features::kCloudLogger});
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  chromeos::cfm::FakeServiceConnectionImpl fake_service_connection_;
  FakeCfmLoggerServiceDelegate logger_delegate_;
};

// This test ensures that the CfmBrowserService is discoverable by its mojom
// name by sending a signal received by CfmHotlineClient.
TEST_F(CfmLoggerServiceTest, CfmLoggerServiceAvailable) {
  GetLoggerRemote();
  ASSERT_TRUE(GetClient()->FakeEmitSignal(mojom::MeetDevicesLogger::Name_));
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

// This test default behavior of the state observer when enabled.
TEST_F(CfmLoggerServiceTest, CfmLoggerServiceStateObserver) {
  auto remote = GetLoggerRemote();

  base::RunLoop observer_loop;
  FakeLoggerStateObserver observer;
  observer.on_notify_state_callback_ =
      base::BindLambdaForTesting([&](mojom::LoggerState state) {
        EXPECT_EQ(state, mojom::LoggerState::kUninitialized);
        observer_loop.Quit();
      });
  remote->AddStateObserver(observer.receiver_.BindNewPipeAndPassRemote());
  observer_loop.Run();

  EXPECT_EQ(observer.on_notify_state_count_, 1);
}

// This test functionality of the state observer when disabled.
TEST_F(CfmLoggerServiceTest, CfmLoggerServiceDisabledStateObserver) {
  DisableLoggerFeature();
  auto remote = GetLoggerRemote();

  base::RunLoop observer_loop;
  FakeLoggerStateObserver observer;
  observer.on_notify_state_callback_ =
      base::BindLambdaForTesting([&](mojom::LoggerState state) {
        EXPECT_EQ(state, mojom::LoggerState::kDisabled);
        observer_loop.Quit();
      });
  remote->AddStateObserver(observer.receiver_.BindNewPipeAndPassRemote());
  observer_loop.Run();

  EXPECT_EQ(observer.on_notify_state_count_, 1);
}

// This test functionality of enqueue.
TEST_F(CfmLoggerServiceTest, CfmLoggerServiceEnqueue) {
  auto remote = GetLoggerRemote();

  base::RunLoop enqueue_loop;
  remote->Enqueue(
      "foo", mojom::EnqueuePriority::kHigh,
      base::BindLambdaForTesting([&](mojom::LoggerStatusPtr status) {
        ASSERT_EQ(mojom::LoggerErrorCode::kOk, status->code);
        enqueue_loop.Quit();
      }));
  enqueue_loop.Run();
}

// This test functionality of enqueue when disabled.
TEST_F(CfmLoggerServiceTest, CfmLoggerServiceDisabledEnqueue) {
  DisableLoggerFeature();
  constexpr auto kUnimplemented = mojom::LoggerErrorCode::kUnimplemented;

  auto remote = GetLoggerRemote();

  base::RunLoop enqueue_loop;
  remote->Enqueue(
      "foo", mojom::EnqueuePriority::kHigh,
      base::BindLambdaForTesting([&](mojom::LoggerStatusPtr status) {
        ASSERT_EQ(kUnimplemented, status->code);
        enqueue_loop.Quit();
      }));
  enqueue_loop.Run();
}

}  // namespace
}  // namespace ash::cfm

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_signaler.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "ash/webui/eche_app_ui/system_info.h"
#include "ash/webui/eche_app_ui/system_info_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash::eche_app {

namespace {

class TaskRunner {
 public:
  TaskRunner() = default;
  ~TaskRunner() = default;

  void WaitForResult() { run_loop_.Run(); }

  void Finish() { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
};

class FakeExchangerClient : public mojom::SignalingMessageExchanger {
 public:
  FakeExchangerClient() = default;
  ~FakeExchangerClient() override = default;

  mojo::PendingReceiver<mojom::SignalingMessageExchanger>
  CreatePendingReceiver() {
    return remote_.BindNewPipeAndPassReceiver();
  }

  // mojom::SignalingMessageExchanger:
  void SendSignalingMessage(const std::vector<uint8_t>& signal) override {
    PA_LOG(INFO) << "FakeExchangerClient SendSignalingMessage";
    remote_->SendSignalingMessage(signal);
  }

  // mojom::SignalingMessageExchanger:
  void SetSignalingMessageObserver(
      mojo::PendingRemote<mojom::SignalingMessageObserver> observer) override {
    remote_->SetSignalingMessageObserver(std::move(observer));
  }

  // mojom::SignalingMessageExchanger:
  void TearDownSignaling() override { remote_->TearDownSignaling(); }

 private:
  mojo::Remote<mojom::SignalingMessageExchanger> remote_;
};

class FakeObserver : public mojom::SignalingMessageObserver {
 public:
  FakeObserver(mojo::PendingRemote<mojom::SignalingMessageObserver>* remote,
               TaskRunner* task_runner)
      : receiver_(this, remote->InitWithNewPipeAndPassReceiver()) {
    task_runner_ = task_runner;
  }
  ~FakeObserver() override = default;

  const std::vector<uint8_t>& received_signals() const {
    return received_signals_;
  }

  // mojom::SignalingMessageObserver:
  void OnReceivedSignalingMessage(const std::vector<uint8_t>& signal) override {
    PA_LOG(INFO) << "FakeObserver OnReceivedSignalingMessage";
    for (size_t i = 0; i < signal.size(); i++) {
      received_signals_.push_back(signal[i]);
    }
    task_runner_->Finish();
  }

 private:
  raw_ptr<TaskRunner> task_runner_;
  std::vector<uint8_t> received_signals_;
  mojo::Receiver<mojom::SignalingMessageObserver> receiver_;
};

class FakeEcheConnector : public EcheConnector {
 public:
  explicit FakeEcheConnector(TaskRunner* task_runner) {
    task_runner_ = task_runner;
  }
  ~FakeEcheConnector() override = default;

  const std::vector<proto::ExoMessage>& sent_messages() const {
    return sent_messages_;
  }

  void SendMessage(const proto::ExoMessage message) override {
    sent_messages_.push_back(message);
    task_runner_->Finish();
  }

  void Disconnect() override {}
  void SendAppsSetupRequest() override {}
  void GetAppsAccessStateRequest() override {}
  void AttemptNearbyConnection() override {}

 private:
  raw_ptr<TaskRunner> task_runner_;
  std::vector<proto::ExoMessage> sent_messages_;
};

class FakeSystemInfoProvider : public SystemInfoProvider {
 public:
  FakeSystemInfoProvider(
      std::unique_ptr<SystemInfo> system_info,
      chromeos::network_config::mojom::CrosNetworkConfig* cros_network_config)
      : SystemInfoProvider(std::move(system_info), cros_network_config) {}
  FakeSystemInfoProvider() {
    PA_LOG(INFO) << "echeapi FakeSystemInfoProvider FakeSystemInfoProvider";
  }
  ~FakeSystemInfoProvider() override = default;

  void FetchWifiNetworkSsidHash() override {
    PA_LOG(INFO) << "echeapi FakeSystemInfoProvider FetchWifiNetworkSsidHash";
    // SHA256 hash for the string 'network'
    hashed_wifi_ssid_ =
        "3009be769fb8f956e8413ee9f3e0836e34968bc40457d0a10c549d2edcf00cc1";
  }

  // mojom::SystemInfoProvider:
  void GetSystemInfo(
      base::OnceCallback<void(const std::string&)> callback) override {}
  void SetSystemInfoObserver(
      mojo::PendingRemote<mojom::SystemInfoObserver> observer) override {}
  void Bind(mojo::PendingReceiver<mojom::SystemInfoProvider> receiver) {}
};

}  // namespace

class EcheSignalerTest : public AshTestBase {
 protected:
  EcheSignalerTest() = default;
  EcheSignalerTest(const EcheSignalerTest&) = delete;
  EcheSignalerTest& operator=(const EcheSignalerTest&) = delete;
  ~EcheSignalerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheNetworkConnectionState},
        /*disabled_features=*/{});
    DCHECK(test_web_view_factory_.get());
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    eche_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();
    eche_connection_status_handler_ =
        std::make_unique<eche_app::EcheConnectionStatusHandler>();
    apps_launch_info_provider_ = std::make_unique<AppsLaunchInfoProvider>(
        eche_connection_status_handler_.get());
    signaler_ = std::make_unique<EcheSignaler>(
        &fake_connector_, &fake_connection_manager_,
        apps_launch_info_provider_.get(),
        eche_connection_status_handler_.get());
  }

  void TearDown() override {
    signaler_.reset();
    apps_launch_info_provider_.reset();
    eche_connection_status_handler_.reset();
    AshTestBase::TearDown();
  }

  EcheSignaler* signaler() { return signaler_.get(); }

  EcheConnectionStatusHandler* eche_connection_status_handler() {
    return eche_connection_status_handler_.get();
  }

  std::vector<uint8_t> getSignal(std::string data) {
    std::vector<uint8_t> signal(data.begin(), data.end());
    return signal;
  }

  proto::ExoMessage getExoMessage(std::string data) {
    std::vector<uint8_t> signal(data.begin(), data.end());
    std::string encoded_signal(signal.begin(), signal.end());
    proto::SignalingRequest request;
    request.set_data(encoded_signal);
    proto::ExoMessage message;
    *message.mutable_request() = std::move(request);
    return message;
  }

  proto::ExoMessage getTearDownSignalingMessage() const {
    proto::SignalingAction action;
    action.set_action_type(proto::ActionType::ACTION_TEAR_DOWN);
    proto::ExoMessage message;
    *message.mutable_action() = std::move(action);
    return message;
  }

  proto::ExoMessage getResponseMessage(std::string data,
                                       std::string ssid,
                                       bool mobile_network) {
    std::vector<uint8_t> signal(data.begin(), data.end());
    std::string encoded_signal(signal.begin(), signal.end());

    proto::ExoMessage message;
    proto::SignalingResponse* response = message.mutable_response();
    response->set_data(std::move(encoded_signal));
    proto::NetworkInfo* network_info = response->mutable_network_info();
    network_info->set_mobile_network(mobile_network);
    network_info->set_ssid(std::move(ssid));

    return message;
  }

  void SetConnectionStatus(secure_channel::ConnectionManager::Status status) {
    fake_connection_manager_.SetStatus(status);
  }

  TaskRunner task_runner_;
  FakeEcheConnector fake_connector_{&task_runner_};
  base::test::ScopedFeatureList feature_list_;

 private:
  raw_ptr<EcheTray, DanglingUntriaged> eche_tray_ = nullptr;
  secure_channel::FakeConnectionManager fake_connection_manager_;
  std::unique_ptr<EcheConnectionStatusHandler> eche_connection_status_handler_;
  std::unique_ptr<AppsLaunchInfoProvider> apps_launch_info_provider_;
  std::unique_ptr<EcheSignaler> signaler_;
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

// Tests SendSignalingMessage.
TEST_F(EcheSignalerTest, TestSendSignalingMessage) {
  FakeExchangerClient fake_exchanger_client;
  signaler()->Bind(fake_exchanger_client.CreatePendingReceiver());
  std::vector<uint8_t> signal = getSignal("123");

  fake_exchanger_client.SendSignalingMessage(signal);
  task_runner_.WaitForResult();

  EXPECT_GT(fake_connector_.sent_messages().size(), (unsigned long)0);
}

// Tests TearDownSignaling.
TEST_F(EcheSignalerTest, TestTearDownSignaling) {
  FakeExchangerClient fake_exchanger_client;
  signaler()->Bind(fake_exchanger_client.CreatePendingReceiver());
  proto::ExoMessage tear_down_signaling_message = getTearDownSignalingMessage();

  fake_exchanger_client.TearDownSignaling();
  task_runner_.WaitForResult();

  EXPECT_EQ(fake_connector_.sent_messages()[0].SerializeAsString(),
            tear_down_signaling_message.SerializeAsString());
}

// Tests SetSignalingMessageObserver and observer should be triggered when
// message is received.
TEST_F(EcheSignalerTest, TestSetSignalingMessageObserverAndReceiveMessage) {
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  proto::ExoMessage message = getExoMessage("123");

  signaler()->SetSignalingMessageObserver(std::move(observer));
  signaler()->OnMessageReceived(message.SerializeAsString());
  task_runner_.WaitForResult();

  EXPECT_GT(fake_observer.received_signals().size(), (unsigned long)0);
}

TEST_F(EcheSignalerTest, TestConnectionFailWhenNoReceiveAnyMessage) {
  base::HistogramTester histograms;
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingNotTriggered, 0);

  signaler()->SetSignalingMessageObserver(std::move(observer));
  signaler()->RecordSignalingTimeout();

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingNotTriggered, 1);
  EXPECT_EQ(fake_observer.received_signals().size(), (unsigned long)0);
}

TEST_F(EcheSignalerTest, TestConnectionFailWhenSignalingHasLateRequest) {
  base::HistogramTester histograms;
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  proto::ExoMessage message = getExoMessage("123");
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingHasLateRequest, 0);

  signaler()->SetSignalingMessageObserver(std::move(observer));
  signaler()->OnMessageReceived(message.SerializeAsString());
  task_runner_.WaitForResult();
  signaler()->RecordSignalingTimeout();

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingHasLateRequest, 1);
  EXPECT_GT(fake_observer.received_signals().size(), (unsigned long)0);
}

TEST_F(EcheSignalerTest, TestConnectionFailWhenSecurityChannelDisconnected) {
  base::HistogramTester histograms;
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSecurityChannelDisconnected, 0);

  signaler()->SetSignalingMessageObserver(std::move(observer));
  signaler()->RecordSignalingTimeout();

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSecurityChannelDisconnected, 1);
}

TEST_F(EcheSignalerTest, TestConnectionFailWhenWiFiNetworksDifferent) {
  base::HistogramTester histograms;
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  FakeSystemInfoProvider fake_system_info_provider;
  proto::ExoMessage message = getResponseMessage(
      "123", "4f7beaf7ab9d5e2c52a2faa3aef34560ad49071957d3029800e85f42931cd5ab",
      false);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kConnectionFailSsidDifferent, 0);

  signaler()->SetSignalingMessageObserver(std::move(observer));
  signaler()->SetSystemInfoProvider(&fake_system_info_provider);
  signaler()->OnMessageReceived(message.SerializeAsString());
  task_runner_.WaitForResult();
  signaler()->RecordSignalingTimeout();

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kConnectionFailSsidDifferent, 1);
  EXPECT_GT(fake_observer.received_signals().size(), (unsigned long)0);
}

TEST_F(EcheSignalerTest, TestConnectionFailWhenWiFiNetworksSame) {
  base::HistogramTester histograms;
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  FakeSystemInfoProvider fake_system_info_provider;
  proto::ExoMessage message = getResponseMessage(
      "123", "3009be769fb8f956e8413ee9f3e0836e34968bc40457d0a10c549d2edcf00cc1",
      false);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kConnectionFailSsidDifferent, 0);

  signaler()->SetSignalingMessageObserver(std::move(observer));
  signaler()->SetSystemInfoProvider(&fake_system_info_provider);
  signaler()->OnMessageReceived(message.SerializeAsString());
  task_runner_.WaitForResult();
  signaler()->RecordSignalingTimeout();

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingHasLateResponse, 1);
  EXPECT_GT(fake_observer.received_signals().size(), (unsigned long)0);
}

TEST_F(EcheSignalerTest, TestConnectionFailWhenRemoteDeviceOnCellular) {
  base::HistogramTester histograms;
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  FakeSystemInfoProvider fake_system_info_provider;
  proto::ExoMessage message = getResponseMessage("123", "network", true);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kConnectionFailRemoteDeviceOnCellular, 0);

  signaler()->SetSignalingMessageObserver(std::move(observer));
  signaler()->SetSystemInfoProvider(&fake_system_info_provider);
  signaler()->OnMessageReceived(message.SerializeAsString());
  task_runner_.WaitForResult();
  signaler()->RecordSignalingTimeout();

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kConnectionFailRemoteDeviceOnCellular, 1);
  EXPECT_GT(fake_observer.received_signals().size(), (unsigned long)0);
}

TEST_F(EcheSignalerTest, TestConnectionFailWhenSignalingHasLateResponse) {
  base::HistogramTester histograms;
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  FakeSystemInfoProvider fake_system_info_provider;
  proto::ExoMessage message = getResponseMessage(
      "123", "3009be769fb8f956e8413ee9f3e0836e34968bc40457d0a10c549d2edcf00cc1",
      false);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingHasLateResponse, 0);

  signaler()->SetSignalingMessageObserver(std::move(observer));
  signaler()->SetSystemInfoProvider(&fake_system_info_provider);
  signaler()->OnMessageReceived(message.SerializeAsString());
  task_runner_.WaitForResult();
  signaler()->RecordSignalingTimeout();

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingHasLateResponse, 1);
  EXPECT_GT(fake_observer.received_signals().size(), (unsigned long)0);
}

TEST_F(EcheSignalerTest, OnRequestCloseConnnectionDoesNotStreamEventFailures) {
  base::HistogramTester histograms;
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  FakeSystemInfoProvider fake_system_info_provider;
  proto::ExoMessage message = getResponseMessage(
      "123", "3009be769fb8f956e8413ee9f3e0836e34968bc40457d0a10c549d2edcf00cc1",
      false);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  histograms.ExpectTotalCount("Eche.StreamEvent.ConnectionFail", 0);

  signaler()->SetSignalingMessageObserver(std::move(observer));
  signaler()->SetSystemInfoProvider(&fake_system_info_provider);
  signaler()->OnMessageReceived(message.SerializeAsString());
  eche_connection_status_handler()->NotifyRequestCloseConnection();
  task_runner_.WaitForResult();

  histograms.ExpectTotalCount("Eche.StreamEvent.ConnectionFail", 0);
  EXPECT_FALSE(signaler()->signaling_timeout_timer_for_test());
}

}  // namespace ash::eche_app

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_signaler.h"

#include <memory>
#include <vector>

#include "ash/system/eche/eche_tray.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {

namespace {

class TaskRunner {
 public:
  TaskRunner() = default;
  ~TaskRunner() = default;

  void WaitForResult() { run_loop_.Run(); }

  void Finish() { run_loop_.Quit(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
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
  TaskRunner* task_runner_;
  std::vector<uint8_t> received_signals_;
  mojo::Receiver<mojom::SignalingMessageObserver> receiver_;
};

class FakeEcheConnector : public EcheConnector {
 public:
  FakeEcheConnector(TaskRunner* task_runner) { task_runner_ = task_runner; }
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
  TaskRunner* task_runner_;
  std::vector<proto::ExoMessage> sent_messages_;
};

}  // namespace

class EcheSignalerTest : public testing::Test {
 protected:
  EcheSignalerTest() = default;
  EcheSignalerTest(const EcheSignalerTest&) = delete;
  EcheSignalerTest& operator=(const EcheSignalerTest&) = delete;
  ~EcheSignalerTest() override = default;

  // testing::Test:
  void SetUp() override {
    signaler_ = std::make_unique<EcheSignaler>(&fake_connector_,
                                               &fake_connection_manager_);
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

  proto::ExoMessage getResponseMessage(std::string data) {
    std::vector<uint8_t> signal(data.begin(), data.end());
    std::string encoded_signal(signal.begin(), signal.end());
    proto::SignalingResponse response;
    response.set_data(encoded_signal);
    proto::ExoMessage message;
    *message.mutable_response() = std::move(response);
    return message;
  }

  void SetConnectionStatus(secure_channel::ConnectionManager::Status status) {
    fake_connection_manager_.SetStatus(status);
  }

  TaskRunner task_runner_;
  FakeEcheConnector fake_connector_{&task_runner_};
  secure_channel::FakeConnectionManager fake_connection_manager_;

  std::unique_ptr<EcheSignaler> signaler_;
};

// Tests SendSignalingMessage.
TEST_F(EcheSignalerTest, TestSendSignalingMessage) {
  FakeExchangerClient fake_exchanger_client;
  signaler_->Bind(fake_exchanger_client.CreatePendingReceiver());
  std::vector<uint8_t> signal = getSignal("123");

  fake_exchanger_client.SendSignalingMessage(signal);
  task_runner_.WaitForResult();

  EXPECT_TRUE(fake_connector_.sent_messages().size() > 0);
}

// Tests TearDownSignaling.
TEST_F(EcheSignalerTest, TestTearDownSignaling) {
  FakeExchangerClient fake_exchanger_client;
  signaler_->Bind(fake_exchanger_client.CreatePendingReceiver());
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

  signaler_->SetSignalingMessageObserver(std::move(observer));
  signaler_->OnMessageReceived(message.SerializeAsString());
  task_runner_.WaitForResult();

  EXPECT_TRUE(fake_observer.received_signals().size() > 0);
}

TEST_F(EcheSignalerTest, TestConnectionFailWhenNoReceiveAnyMessage) {
  base::HistogramTester histograms;
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingNotTriggered, 0);

  signaler_->SetSignalingMessageObserver(std::move(observer));
  signaler_->RecordSignalingTimeout();

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingNotTriggered, 1);
  EXPECT_TRUE(fake_observer.received_signals().size() == 0);
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

  signaler_->SetSignalingMessageObserver(std::move(observer));
  signaler_->OnMessageReceived(message.SerializeAsString());
  task_runner_.WaitForResult();
  signaler_->RecordSignalingTimeout();

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingHasLateRequest, 1);
  EXPECT_TRUE(fake_observer.received_signals().size() > 0);
}

TEST_F(EcheSignalerTest, TestConnectionFailWhenSignalingHasLateResponse) {
  base::HistogramTester histograms;
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  proto::ExoMessage message = getResponseMessage("123");
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingHasLateResponse, 0);

  signaler_->SetSignalingMessageObserver(std::move(observer));
  signaler_->OnMessageReceived(message.SerializeAsString());
  task_runner_.WaitForResult();
  signaler_->RecordSignalingTimeout();

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSignalingHasLateResponse, 1);
  EXPECT_TRUE(fake_observer.received_signals().size() > 0);
}

TEST_F(EcheSignalerTest, TestConnectionFailWhenSecurityChannelDisconnected) {
  base::HistogramTester histograms;
  mojo::PendingRemote<mojom::SignalingMessageObserver> observer;
  FakeObserver fake_observer(&observer, &task_runner_);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSecurityChannelDisconnected, 0);

  signaler_->SetSignalingMessageObserver(std::move(observer));
  signaler_->RecordSignalingTimeout();

  histograms.ExpectUniqueSample(
      "Eche.StreamEvent.ConnectionFail",
      EcheTray::ConnectionFailReason::kSecurityChannelDisconnected, 1);
}

}  // namespace eche_app
}  // namespace ash

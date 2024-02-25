// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/secure_channel/nearby_connector_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/secure_channel/fake_nearby_connection_broker.h"
#include "chrome/browser/ash/secure_channel/fake_nearby_endpoint_finder.h"
#include "chrome/browser/ash/secure_channel/nearby_connection_broker_impl.h"
#include "chrome/browser/ash/secure_channel/nearby_endpoint_finder_impl.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_nearby_process_manager.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace secure_channel {
namespace {

int g_next_message_receiver_id = 0;

const std::vector<uint8_t> GetEid() {
  return std::vector<uint8_t>{0, 1};
}

std::vector<uint8_t> GetBluetoothAddress(uint8_t repeated_address_value) {
  return std::vector<uint8_t>(6u, repeated_address_value);
}

class FakeEndpointFinderFactory : public NearbyEndpointFinderImpl::Factory {
 public:
  FakeEndpointFinderFactory() = default;
  ~FakeEndpointFinderFactory() override = default;

 private:
  // NearbyEndpointFinderImpl::Factory:
  std::unique_ptr<NearbyEndpointFinder> CreateInstance(
      const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
          nearby_connections) override {
    return std::make_unique<FakeNearbyEndpointFinder>();
  }
};

class FakeConnectionBrokerFactory : public NearbyConnectionBrokerImpl::Factory {
 public:
  FakeConnectionBrokerFactory() = default;
  ~FakeConnectionBrokerFactory() override = default;

  FakeNearbyConnectionBroker* last_created() { return last_created_; }

 private:
  // NearbyConnectionBrokerImpl::Factory:
  std::unique_ptr<NearbyConnectionBroker> CreateInstance(
      const std::vector<uint8_t>& bluetooth_public_address,
      NearbyEndpointFinder* endpoint_finder,
      mojo::PendingReceiver<mojom::NearbyMessageSender> message_sender_receiver,
      mojo::PendingReceiver<mojom::NearbyFilePayloadHandler>
          file_payload_handler_receiver,
      mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver_remote,
      mojo::PendingRemote<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener,
      const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
          nearby_connections,
      base::OnceClosure on_connected_callback,
      base::OnceClosure on_disconnected_callback,
      std::unique_ptr<base::OneShotTimer> timer) override {
    auto instance = std::make_unique<FakeNearbyConnectionBroker>(
        bluetooth_public_address, std::move(message_sender_receiver),
        std::move(file_payload_handler_receiver),
        std::move(message_receiver_remote),
        std::move(nearby_connection_state_listener),
        std::move(on_connected_callback), std::move(on_disconnected_callback));
    last_created_ = instance.get();
    return instance;
  }

  raw_ptr<FakeNearbyConnectionBroker, DanglingUntriaged> last_created_ =
      nullptr;
};

class FakeMessageReceiver : public mojom::NearbyMessageReceiver {
 public:
  FakeMessageReceiver() = default;
  ~FakeMessageReceiver() override = default;

  int id() const { return id_; }
  const std::vector<std::string>& received_messages() const {
    return received_messages_;
  }

  mojo::PendingRemote<mojom::NearbyMessageReceiver> GeneratePendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void set_on_message_received(base::OnceClosure on_message_received) {
    on_message_received_ = std::move(on_message_received);
  }

  void SetMojoDisconnectHandler(base::OnceClosure on_disconnected) {
    receiver_.set_disconnect_handler(std::move(on_disconnected));
  }

 private:
  // mojom::NearbyMessageReceiver:
  void OnMessageReceived(const std::string& message) override {
    received_messages_.push_back(message);
    std::move(on_message_received_).Run();
  }

  int id_ = g_next_message_receiver_id++;
  mojo::Receiver<mojom::NearbyMessageReceiver> receiver_{this};
  base::OnceClosure on_message_received_;
  std::vector<std::string> received_messages_;
};

class FakeNearbyConnectionStateListener
    : public mojom::NearbyConnectionStateListener {
 public:
  FakeNearbyConnectionStateListener() = default;
  ~FakeNearbyConnectionStateListener() override = default;

  mojo::PendingRemote<mojom::NearbyConnectionStateListener>
  GeneratePendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  void OnNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep nearby_connection_step,
      mojom::NearbyConnectionStepResult result) override {
    nearby_connection_step_ = nearby_connection_step;
    nearby_connection_step_result_ = result;
  }

  mojo::Receiver<mojom::NearbyConnectionStateListener> receiver_{this};
  mojom::NearbyConnectionStep nearby_connection_step_;
  mojom::NearbyConnectionStepResult nearby_connection_step_result_;
};

}  // namespace

class NearbyConnectorImplTest : public testing::Test {
 protected:
  NearbyConnectorImplTest() = default;
  ~NearbyConnectorImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    NearbyConnectionBrokerImpl::Factory::SetFactoryForTesting(
        &fake_connection_broker_factory_);
    NearbyEndpointFinderImpl::Factory::SetFactoryForTesting(
        &fake_endpoint_finder_factory_);

    connector_ =
        std::make_unique<NearbyConnectorImpl>(&fake_nearby_process_manager_);
  }

  void TearDown() override {
    NearbyConnectionBrokerImpl::Factory::SetFactoryForTesting(nullptr);
    NearbyEndpointFinderImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void Connect(
      FakeMessageReceiver* fake_message_receiver,
      FakeNearbyConnectionStateListener* fake_nearby_connection_state_listener,
      const std::vector<uint8_t>& address) {
    connector_->Connect(
        address, GetEid(), fake_message_receiver->GeneratePendingRemote(),
        fake_nearby_connection_state_listener->GeneratePendingRemote(),
        base::BindOnce(&NearbyConnectorImplTest::OnConnectResult,
                       base::Unretained(this), fake_message_receiver->id()));
  }

  // Invoked when OnConnectResult() is called.
  base::OnceClosure on_connect_;

  // Keyed by FakeMessageReceiver::id().
  base::flat_map<int, mojo::Remote<mojom::NearbyMessageSender>>
      id_to_remote_map_;

  FakeConnectionBrokerFactory fake_connection_broker_factory_;
  nearby::FakeNearbyProcessManager fake_nearby_process_manager_;

 private:
  void OnConnectResult(int id,
                       mojo::PendingRemote<mojom::NearbyMessageSender>
                           message_sender_pending_remote,
                       mojo::PendingRemote<mojom::NearbyFilePayloadHandler>
                           file_payload_handler_remote) {
    if (!message_sender_pending_remote)
      id_to_remote_map_[id] = mojo::Remote<mojom::NearbyMessageSender>();
    else
      id_to_remote_map_[id].Bind(std::move(message_sender_pending_remote));

    std::move(on_connect_).Run();
  }

  base::test::TaskEnvironment task_environment_;

  FakeEndpointFinderFactory fake_endpoint_finder_factory_;

  std::unique_ptr<NearbyConnector> connector_;
};

TEST_F(NearbyConnectorImplTest, ConnectAndTransferMessages) {
  // Attempt connection.
  FakeMessageReceiver receiver;
  FakeNearbyConnectionStateListener nearby_connection_state_listener;
  Connect(&receiver, &nearby_connection_state_listener,
          GetBluetoothAddress(/*repeated_address_value=*/1u));
  FakeNearbyConnectionBroker* broker =
      fake_connection_broker_factory_.last_created();
  EXPECT_EQ(1u, fake_nearby_process_manager_.GetNumActiveReferences());

  // Complete connection.
  base::RunLoop connect_run_loop;
  on_connect_ = connect_run_loop.QuitClosure();
  broker->NotifyConnected();
  connect_run_loop.Run();

  // Send a message.
  base::RunLoop send_run_loop;
  id_to_remote_map_[receiver.id()]->SendMessage(
      "hi", base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        send_run_loop.Quit();
      }));
  send_run_loop.Run();
  EXPECT_EQ("hi", broker->sent_messages()[0]);

  // Receive a message.
  base::RunLoop receive_run_loop;
  receiver.set_on_message_received(receive_run_loop.QuitClosure());
  broker->NotifyMessageReceived("bye");
  receive_run_loop.Run();
  EXPECT_EQ("bye", receiver.received_messages()[0]);

  // Disconnect.
  base::RunLoop disconnect_run_loop;
  receiver.SetMojoDisconnectHandler(disconnect_run_loop.QuitClosure());
  broker->InvokeDisconnectedCallback();
  disconnect_run_loop.Run();
  EXPECT_EQ(0u, fake_nearby_process_manager_.GetNumActiveReferences());
}

TEST_F(NearbyConnectorImplTest, TwoConnections) {
  // Attempt connection 1.
  FakeMessageReceiver receiver1;
  FakeNearbyConnectionStateListener nearby_connection_state_listener1;
  Connect(&receiver1, &nearby_connection_state_listener1,
          GetBluetoothAddress(/*repeated_address_value=*/1u));
  FakeNearbyConnectionBroker* broker1 =
      fake_connection_broker_factory_.last_created();
  EXPECT_EQ(1u, fake_nearby_process_manager_.GetNumActiveReferences());

  // Attempt connection 2 before connection 1 has completed. No new broker
  // should have been created since they are queued.
  FakeMessageReceiver receiver2;
  FakeNearbyConnectionStateListener nearby_connection_state_listener2;
  Connect(&receiver2, &nearby_connection_state_listener2,
          GetBluetoothAddress(/*repeated_address_value=*/2u));
  EXPECT_EQ(broker1, fake_connection_broker_factory_.last_created());
  EXPECT_EQ(1u, fake_nearby_process_manager_.GetNumActiveReferences());

  // Complete connection 1.
  base::RunLoop connect_run_loop1;
  on_connect_ = connect_run_loop1.QuitClosure();
  broker1->NotifyConnected();
  connect_run_loop1.Run();

  // A new broker should have been created for connection 2 since connection 1
  // has completed.
  FakeNearbyConnectionBroker* broker2 =
      fake_connection_broker_factory_.last_created();
  EXPECT_NE(broker1, broker2);

  // Complete connection 2.
  base::RunLoop connect_run_loop2;
  on_connect_ = connect_run_loop2.QuitClosure();
  broker2->NotifyConnected();
  connect_run_loop2.Run();

  // Disconnect connection 1. The process reference should still be active since
  // there is still an active connection.
  base::RunLoop disconnect_run_loop1;
  receiver1.SetMojoDisconnectHandler(disconnect_run_loop1.QuitClosure());
  broker1->InvokeDisconnectedCallback();
  disconnect_run_loop1.Run();
  EXPECT_EQ(1u, fake_nearby_process_manager_.GetNumActiveReferences());

  // Disconnect connection 2. The process reference should have been released.
  base::RunLoop disconnect_run_loop2;
  receiver2.SetMojoDisconnectHandler(disconnect_run_loop2.QuitClosure());
  broker2->InvokeDisconnectedCallback();
  disconnect_run_loop2.Run();
  EXPECT_EQ(0u, fake_nearby_process_manager_.GetNumActiveReferences());
}

// Regression test for https://crbug.com/1156162.
TEST_F(NearbyConnectorImplTest, TwoConnections_FirstFails) {
  // Attempt connection 1.
  FakeMessageReceiver receiver1;
  FakeNearbyConnectionStateListener nearby_connection_state_listener1;
  Connect(&receiver1, &nearby_connection_state_listener1,
          GetBluetoothAddress(/*repeated_address_value=*/1u));
  FakeNearbyConnectionBroker* broker1 =
      fake_connection_broker_factory_.last_created();
  EXPECT_EQ(1u, fake_nearby_process_manager_.GetNumActiveReferences());

  // Attempt connection 2 before connection 1 has completed. No new broker
  // should have been created since they are queued.
  FakeMessageReceiver receiver2;
  FakeNearbyConnectionStateListener nearby_connection_state_listener2;
  Connect(&receiver2, &nearby_connection_state_listener2,
          GetBluetoothAddress(/*repeated_address_value=*/2u));
  EXPECT_EQ(broker1, fake_connection_broker_factory_.last_created());
  EXPECT_EQ(1u, fake_nearby_process_manager_.GetNumActiveReferences());

  // Fail connection 1 by calling Disconnect() before completing the connection.
  base::RunLoop connect_run_loop;
  on_connect_ = connect_run_loop.QuitClosure();
  base::RunLoop disconnect_run_loop;
  receiver1.SetMojoDisconnectHandler(disconnect_run_loop.QuitClosure());
  broker1->InvokeDisconnectedCallback();
  connect_run_loop.Run();
  disconnect_run_loop.Run();

  // A new broker should have been created for connection 2 since connection 1
  // has completed.
  FakeNearbyConnectionBroker* broker2 =
      fake_connection_broker_factory_.last_created();
  EXPECT_EQ(1u, fake_nearby_process_manager_.GetNumActiveReferences());

  // Complete connection 2.
  base::RunLoop connect_run_loop2;
  on_connect_ = connect_run_loop2.QuitClosure();
  broker2->NotifyConnected();
  connect_run_loop2.Run();

  // Disconnect connection 2. The process reference should have been released.
  base::RunLoop disconnect_run_loop2;
  receiver2.SetMojoDisconnectHandler(disconnect_run_loop2.QuitClosure());
  broker2->InvokeDisconnectedCallback();
  disconnect_run_loop2.Run();
  EXPECT_EQ(0u, fake_nearby_process_manager_.GetNumActiveReferences());
}

TEST_F(NearbyConnectorImplTest, FailToConnect) {
  // Attempt connection.
  FakeMessageReceiver receiver;
  FakeNearbyConnectionStateListener nearby_connection_state_listener;
  Connect(&receiver, &nearby_connection_state_listener,
          GetBluetoothAddress(/*repeated_address_value=*/1u));
  FakeNearbyConnectionBroker* broker =
      fake_connection_broker_factory_.last_created();
  EXPECT_EQ(1u, fake_nearby_process_manager_.GetNumActiveReferences());

  // Fail connection by calling Disconnect() before completing the connection.
  base::RunLoop connect_run_loop;
  on_connect_ = connect_run_loop.QuitClosure();
  base::RunLoop disconnect_run_loop;
  receiver.SetMojoDisconnectHandler(disconnect_run_loop.QuitClosure());
  broker->InvokeDisconnectedCallback();
  connect_run_loop.Run();
  disconnect_run_loop.Run();

  // The mojo::Remote<mojom::NearbyMessageSender> should be null.
  EXPECT_EQ(0u, fake_nearby_process_manager_.GetNumActiveReferences());
  EXPECT_FALSE(id_to_remote_map_[receiver.id()]);
}

TEST_F(NearbyConnectorImplTest, NearbyProcessStops) {
  // Attempt connection.
  FakeMessageReceiver receiver;
  FakeNearbyConnectionStateListener nearby_connection_state_listener;
  Connect(&receiver, &nearby_connection_state_listener,
          GetBluetoothAddress(/*repeated_address_value=*/1u));
  FakeNearbyConnectionBroker* broker =
      fake_connection_broker_factory_.last_created();
  EXPECT_EQ(1u, fake_nearby_process_manager_.GetNumActiveReferences());

  // Complete connection.
  base::RunLoop connect_run_loop;
  on_connect_ = connect_run_loop.QuitClosure();
  broker->NotifyConnected();
  connect_run_loop.Run();

  // Stop the Nearby process; this should result in a disconnection.
  base::RunLoop disconnect_run_loop;
  receiver.SetMojoDisconnectHandler(disconnect_run_loop.QuitClosure());
  fake_nearby_process_manager_.SimulateProcessStopped(
      nearby::NearbyProcessManager::NearbyProcessShutdownReason::kNormal);
  disconnect_run_loop.Run();
  EXPECT_EQ(0u, fake_nearby_process_manager_.GetNumActiveReferences());
}

TEST_F(NearbyConnectorImplTest, NearbyProcessStopsDuringConnectionAttempt) {
  // Attempt connection.
  FakeMessageReceiver receiver;
  FakeNearbyConnectionStateListener nearby_connection_state_listener;
  Connect(&receiver, &nearby_connection_state_listener,
          GetBluetoothAddress(/*repeated_address_value=*/1u));
  EXPECT_EQ(1u, fake_nearby_process_manager_.GetNumActiveReferences());

  // Stop the Nearby process; this should result in a disconnection.
  base::RunLoop connect_run_loop;
  on_connect_ = connect_run_loop.QuitClosure();
  base::RunLoop disconnect_run_loop;
  receiver.SetMojoDisconnectHandler(disconnect_run_loop.QuitClosure());
  fake_nearby_process_manager_.SimulateProcessStopped(
      nearby::NearbyProcessManager::NearbyProcessShutdownReason::kNormal);
  connect_run_loop.Run();
  disconnect_run_loop.Run();
  EXPECT_EQ(0u, fake_nearby_process_manager_.GetNumActiveReferences());
}

}  // namespace secure_channel
}  // namespace ash

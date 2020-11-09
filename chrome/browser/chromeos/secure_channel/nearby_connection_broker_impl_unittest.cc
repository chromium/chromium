// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/secure_channel/nearby_connection_broker_impl.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/secure_channel/fake_nearby_endpoint_finder.h"
#include "chromeos/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace secure_channel {
namespace {

using location::nearby::connections::mojom::ConnectionInfo;
using location::nearby::connections::mojom::ConnectionLifecycleListener;
using location::nearby::connections::mojom::ConnectionOptionsPtr;
using location::nearby::connections::mojom::DiscoveredEndpointInfo;
using location::nearby::connections::mojom::EndpointDiscoveryListener;
using location::nearby::connections::mojom::PayloadListener;
using location::nearby::connections::mojom::PayloadPtr;
using location::nearby::connections::mojom::Status;

using testing::_;
using testing::Invoke;

const char kEndpointId[] = "endpointId";

const std::vector<uint8_t>& GetBluetoothAddress() {
  static const std::vector<uint8_t> address{0, 1, 2, 3, 4, 5};
  return address;
}

const std::vector<uint8_t>& GetEndpointInfo() {
  static const std::vector<uint8_t> info{6, 7, 8, 9, 10};
  return info;
}

}  // namespace

class NearbyConnectionBrokerImplTest : public testing::Test,
                                       public mojom::NearbyMessageReceiver {
 protected:
  NearbyConnectionBrokerImplTest() = default;
  ~NearbyConnectionBrokerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    broker_ = NearbyConnectionBrokerImpl::Factory::Create(
        GetBluetoothAddress(), &fake_endpoint_finder_,
        message_sender_.BindNewPipeAndPassReceiver(),
        message_receiver_.BindNewPipeAndPassRemote(),
        mock_nearby_connections_.shared_remote(),
        base::BindOnce(&NearbyConnectionBrokerImplTest::OnConnected,
                       base::Unretained(this)),
        base::BindOnce(&NearbyConnectionBrokerImplTest::OnDisconnected,
                       base::Unretained(this)));
    EXPECT_EQ(GetBluetoothAddress(),
              fake_endpoint_finder_.remote_device_bluetooth_address());
  }

  void DiscoverEndpoint() {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_nearby_connections_, RequestConnection(_, _, _, _, _, _))
        .WillOnce(Invoke(
            [&](const std::string& service_id,
                const std::vector<uint8_t>& endpoint_info,
                const std::string& endpoint_id, ConnectionOptionsPtr options,
                mojo::PendingRemote<ConnectionLifecycleListener> listener,
                NearbyConnectionsMojom::RequestConnectionCallback callback) {
              request_connection_callback_ = std::move(callback);
              connection_lifecycle_listener_.Bind(std::move(listener));
              run_loop.Quit();
            }));

    fake_endpoint_finder_.NotifyEndpointFound(
        kEndpointId,
        DiscoveredEndpointInfo::New(GetEndpointInfo(), mojom::kServiceId));

    run_loop.Run();
  }

  void FailDiscovery() {
    base::RunLoop run_loop;
    on_disconnected_closure_ = run_loop.QuitClosure();
    fake_endpoint_finder_.NotifyEndpointDiscoveryFailure();
    run_loop.Run();
  }

  void InvokeRequestConnectionCallback(bool success) {
    if (!success) {
      base::RunLoop run_loop;
      on_disconnected_closure_ = run_loop.QuitClosure();
      std::move(request_connection_callback_).Run(Status::kError);
      run_loop.Run();
      return;
    }

    std::move(request_connection_callback_).Run(Status::kSuccess);

    // Ensure that callback result is received; cannot use external event
    // because the success callback only updates internal state.
    base::RunLoop().RunUntilIdle();
  }

  void NotifyConnectionInitiated() {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_nearby_connections_, AcceptConnection(_, _, _, _))
        .WillOnce(Invoke(
            [&](const std::string& service_id, const std::string& endpoint_id,
                mojo::PendingRemote<PayloadListener> listener,
                NearbyConnectionsMojom::AcceptConnectionCallback callback) {
              accept_connection_callback_ = std::move(callback);
              payload_listener_listener_.Bind(std::move(listener));
              run_loop.Quit();
            }));

    connection_lifecycle_listener_->OnConnectionInitiated(
        kEndpointId, ConnectionInfo::New());

    run_loop.Run();
  }

  void InvokeAcceptConnectionCallback(bool success) {
    if (!success) {
      base::RunLoop run_loop;
      on_disconnected_closure_ = run_loop.QuitClosure();
      std::move(accept_connection_callback_).Run(Status::kError);
      run_loop.Run();
      return;
    }

    std::move(accept_connection_callback_).Run(Status::kSuccess);

    // Ensure that callback result is received; cannot use external event
    // because the success callback only updates internal state.
    base::RunLoop().RunUntilIdle();
  }

  void NotifyConnectionAccepted() {
    base::RunLoop run_loop;
    on_connected_closure_ = run_loop.QuitClosure();
    connection_lifecycle_listener_->OnConnectionAccepted(kEndpointId);
    run_loop.Run();
  }

  void SetUpFullConnection() {
    DiscoverEndpoint();
    InvokeRequestConnectionCallback(/*success=*/true);
    NotifyConnectionInitiated();
    InvokeAcceptConnectionCallback(/*success=*/true);
    NotifyConnectionAccepted();
  }

  void SendMessage(const std::string& message, bool success) {
    base::RunLoop send_message_run_loop;
    base::RunLoop send_message_response_run_loop;

    NearbyConnectionsMojom::SendPayloadCallback send_payload_callback;
    std::string sent_message;
    EXPECT_CALL(mock_nearby_connections_, SendPayload(_, _, _, _))
        .WillOnce(
            Invoke([&](const std::string& service_id,
                       const std::vector<std::string>& endpoint_ids,
                       PayloadPtr payload,
                       NearbyConnectionsMojom::SendPayloadCallback callback) {
              send_payload_callback = std::move(callback);

              const std::vector<uint8_t>& payload_bytes =
                  payload->content->get_bytes()->bytes;
              sent_message =
                  std::string(payload_bytes.begin(), payload_bytes.end());

              send_message_run_loop.Quit();
            }));

    message_sender_->SendMessage(
        message, base::BindLambdaForTesting([&](bool did_send_succeeed) {
          EXPECT_EQ(success, did_send_succeeed);
          send_message_response_run_loop.Quit();
        }));
    send_message_run_loop.Run();

    EXPECT_EQ(message, sent_message);

    if (success) {
      std::move(send_payload_callback).Run(Status::kSuccess);
      send_message_response_run_loop.Run();
      return;
    }

    // Failure to send should disconnect the ongoing connection.
    base::RunLoop disconnect_run_loop;
    on_disconnected_closure_ = disconnect_run_loop.QuitClosure();
    std::move(send_payload_callback).Run(Status::kError);
    send_message_response_run_loop.Run();
    disconnect_run_loop.Run();
  }

  // TODO(khormoto): Verify received messages once this functionality is
  // implemented.
  std::vector<std::string> received_messages_;

 private:
  // mojom::NearbyMessageReceiver:
  void OnMessageReceived(const std::string& message) override {
    received_messages_.push_back(message);
  }

  void OnConnected() { std::move(on_connected_closure_).Run(); }

  void OnDisconnected() { std::move(on_disconnected_closure_).Run(); }

  base::test::TaskEnvironment task_environment_;
  nearby::MockNearbyConnections mock_nearby_connections_;
  FakeNearbyEndpointFinder fake_endpoint_finder_;

  mojo::Remote<mojom::NearbyMessageSender> message_sender_;
  mojo::Receiver<mojom::NearbyMessageReceiver> message_receiver_{this};

  std::unique_ptr<NearbyConnectionBroker> broker_;

  base::OnceClosure on_connected_closure_;
  base::OnceClosure on_disconnected_closure_;

  NearbyConnectionsMojom::RequestConnectionCallback
      request_connection_callback_;
  NearbyConnectionsMojom::AcceptConnectionCallback accept_connection_callback_;

  mojo::Remote<ConnectionLifecycleListener> connection_lifecycle_listener_;
  mojo::Remote<PayloadListener> payload_listener_listener_;
};

// TODO(khorimoto): Add test for receiving incoming messages.
TEST_F(NearbyConnectionBrokerImplTest, SendAndReceive) {
  SetUpFullConnection();
  SendMessage("test1", /*success=*/true);
  SendMessage("test2", /*success=*/true);
}

TEST_F(NearbyConnectionBrokerImplTest, FailToSend) {
  SetUpFullConnection();
  SendMessage("test", /*success=*/false);
}

TEST_F(NearbyConnectionBrokerImplTest, FailDiscovery) {
  FailDiscovery();
}

TEST_F(NearbyConnectionBrokerImplTest, FailRequestingConnection) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(/*success=*/false);
}

TEST_F(NearbyConnectionBrokerImplTest, FailAcceptingConnection) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(/*success=*/true);
  NotifyConnectionInitiated();
  InvokeAcceptConnectionCallback(/*success=*/false);
}

}  // namespace secure_channel
}  // namespace chromeos

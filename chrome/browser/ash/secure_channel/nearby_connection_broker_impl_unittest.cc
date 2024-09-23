// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/secure_channel/nearby_connection_broker_impl.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ash/secure_channel/fake_nearby_endpoint_finder.h"
#include "chrome/browser/ash/secure_channel/util/histogram_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace secure_channel {
namespace {

using ::nearby::connections::mojom::BytesPayload;
using ::nearby::connections::mojom::ConnectionInfo;
using ::nearby::connections::mojom::ConnectionLifecycleListener;
using ::nearby::connections::mojom::ConnectionOptionsPtr;
using ::nearby::connections::mojom::DiscoveredEndpointInfo;
using ::nearby::connections::mojom::FilePayload;
using ::nearby::connections::mojom::Payload;
using ::nearby::connections::mojom::PayloadContent;
using ::nearby::connections::mojom::PayloadListener;
using ::nearby::connections::mojom::PayloadPtr;
using ::nearby::connections::mojom::PayloadStatus;
using ::nearby::connections::mojom::PayloadTransferUpdate;
using ::nearby::connections::mojom::PayloadTransferUpdatePtr;
using ::nearby::connections::mojom::Status;
using ::testing::_;
using ::testing::Invoke;

const char kEndpointId[] = "endpointId";

const std::vector<uint8_t>& GetEid() {
  static const std::vector<uint8_t> eid{0, 1};
  return eid;
}

const std::vector<uint8_t>& GetBluetoothAddress() {
  static const std::vector<uint8_t> address{0, 1, 2, 3, 4, 5};
  return address;
}

const std::vector<uint8_t>& GetEndpointInfo() {
  static const std::vector<uint8_t> info{6, 7, 8, 9, 10};
  return info;
}

}  // namespace

class NearbyConnectionBrokerImplTest
    : public testing::Test,
      public mojom::NearbyMessageReceiver,
      public mojom::NearbyConnectionStateListener,
      public mojom::FilePayloadListener {
 protected:
  NearbyConnectionBrokerImplTest() = default;
  ~NearbyConnectionBrokerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    broker_ = NearbyConnectionBrokerImpl::Factory::Create(
        GetBluetoothAddress(), GetEid(), &fake_endpoint_finder_,
        message_sender_.BindNewPipeAndPassReceiver(),
        file_payload_handler_.BindNewPipeAndPassReceiver(),
        message_receiver_.BindNewPipeAndPassRemote(),
        nearby_connection_state_listener_.BindNewPipeAndPassRemote(),
        mock_nearby_connections_.shared_remote(),
        base::BindOnce(&NearbyConnectionBrokerImplTest::OnConnected,
                       base::Unretained(this)),
        base::BindOnce(&NearbyConnectionBrokerImplTest::OnDisconnected,
                       base::Unretained(this)),
        std::move(mock_timer));
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
    fake_endpoint_finder_.NotifyEndpointDiscoveryFailure(
        ::nearby::connections::mojom::Status::kAlreadyDiscovering);
    run_loop.Run();
  }

  void InvokeRequestConnectionCallback(Status status) {
    if (status != Status::kSuccess) {
      base::RunLoop run_loop;
      on_disconnected_closure_ = run_loop.QuitClosure();
      std::move(request_connection_callback_).Run(status);
      run_loop.Run();
      return;
    }

    std::move(request_connection_callback_).Run(status);

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
              payload_listener_.Bind(std::move(listener));
              run_loop.Quit();
            }));

    connection_lifecycle_listener_->OnConnectionInitiated(
        kEndpointId, ConnectionInfo::New());

    run_loop.Run();
  }

  void InvokeAcceptConnectionCallback(Status status) {
    if (status != Status::kSuccess) {
      base::RunLoop run_loop;
      ExpectDisconnectFromEndpoint(run_loop.QuitClosure());
      std::move(accept_connection_callback_).Run(status);
      run_loop.Run();
      return;
    }

    std::move(accept_connection_callback_).Run(status);

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
    InvokeRequestConnectionCallback(Status::kSuccess);
    NotifyConnectionInitiated();
    InvokeAcceptConnectionCallback(Status::kSuccess);
    NotifyConnectionAccepted();
  }

  void InvokeDisconnectedCallback() {
    base::RunLoop disconnect_run_loop;
    on_disconnected_closure_ = disconnect_run_loop.QuitClosure();
    connection_lifecycle_listener_->OnDisconnected(kEndpointId);
    disconnect_run_loop.Run();
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
    ExpectDisconnectFromEndpoint(disconnect_run_loop.QuitClosure());
    std::move(send_payload_callback).Run(Status::kError);
    send_message_response_run_loop.Run();
    disconnect_run_loop.Run();
  }

  void ReceiveMessage(const std::string& message) {
    static int64_t next_payload_id = 0;

    base::RunLoop receive_run_loop;
    on_message_received_closure_ = receive_run_loop.QuitClosure();

    std::vector<uint8_t> message_as_bytes(message.begin(), message.end());
    payload_listener_->OnPayloadReceived(
        kEndpointId, Payload::New(next_payload_id++,
                                  PayloadContent::NewBytes(
                                      BytesPayload::New(message_as_bytes))));
    receive_run_loop.Run();

    EXPECT_EQ(received_messages_.back(), message);
  }

  void ReceiveFilePayload(int64_t payload_id, const base::FilePath& file_path) {
    // Create fake file to receive.
    const std::vector<uint8_t> kFakeFileContent{0x01, 0x02, 0x03};
    base::File output_file(file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                          base::File::Flags::FLAG_WRITE);
    output_file.WriteAndCheck(
        /*offset=*/0,
        base::make_span(kFakeFileContent.begin(), kFakeFileContent.end()));
    output_file.Flush();
    output_file.Close();
    base::File input_file(
        file_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);

    payload_listener_->OnPayloadReceived(
        kEndpointId,
        Payload::New(payload_id, PayloadContent::NewFile(
                                     FilePayload::New(std::move(input_file)))));
  }

  void RegisterPayloadFile(int64_t payload_id,
                           const base::FilePath& file_path,
                           bool expect_success) {
    base::File input_file(
        file_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
    base::File output_file(file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                          base::File::Flags::FLAG_WRITE);

    base::RunLoop nearby_connections_run_loop;
    base::RunLoop file_payload_handler_run_loop;

    NearbyConnectionsMojom::RegisterPayloadFileCallback
        register_payload_file_callback;
    EXPECT_CALL(mock_nearby_connections_, RegisterPayloadFile(_, _, _, _, _))
        .WillOnce(Invoke(
            [&](const std::string& service_id, int64_t payload_id,
                const base::File& input_file, const base::File& output_file,
                NearbyConnectionsMojom::RegisterPayloadFileCallback callback) {
              register_payload_file_callback = std::move(callback);
              nearby_connections_run_loop.Quit();
            }));

    file_payload_handler_->RegisterPayloadFile(
        payload_id,
        mojom::PayloadFiles::New(std::move(input_file), std::move(output_file)),
        file_payload_listener_.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](bool success) {
          EXPECT_EQ(expect_success, success);
          file_payload_handler_run_loop.Quit();
        }));
    nearby_connections_run_loop.Run();

    std::move(register_payload_file_callback)
        .Run(expect_success ? Status::kSuccess : Status::kError);
    file_payload_handler_run_loop.Run();
  }

  void ReceiveFileTransferUpdate(PayloadTransferUpdatePtr update) {
    payload_listener_->OnPayloadTransferUpdate(kEndpointId, std::move(update));
    payload_listener_.FlushForTesting();
  }

  void DisconnectMojoBindings(bool expected_to_disconnect) {
    if (!expected_to_disconnect) {
      base::RunLoop disconnect_run_loop;
      on_disconnected_closure_ = disconnect_run_loop.QuitClosure();
      EXPECT_CALL(mock_nearby_connections_, DisconnectFromEndpoint(_, _, _))
          .Times(0);
      message_sender_.reset();
      file_payload_handler_.reset();
      disconnect_run_loop.Run();
      return;
    }

    base::RunLoop disconnect_from_endpoint_run_loop;
    ExpectDisconnectFromEndpoint(
        disconnect_from_endpoint_run_loop.QuitClosure());
    message_sender_.reset();
    file_payload_handler_.reset();
    disconnect_from_endpoint_run_loop.Run();
  }

  void InvokeDisconnectedFromEndpointCallback(bool success) {
    if (success) {
      std::move(disconnect_from_endpoint_callback_).Run(Status::kSuccess);
      // Ensure that callback result is received; cannot use external event
      // because the success callback only updates internal state.
      base::RunLoop().RunUntilIdle();
      return;
    }

    base::RunLoop disconnect_run_loop;
    on_disconnected_closure_ = disconnect_run_loop.QuitClosure();
    std::move(disconnect_from_endpoint_callback_).Run(Status::kError);
    disconnect_run_loop.Run();
  }

  void SimulateTimeout(bool expected_to_disconnect) {
    if (!expected_to_disconnect) {
      base::RunLoop disconnect_run_loop;
      on_disconnected_closure_ = disconnect_run_loop.QuitClosure();
      EXPECT_CALL(mock_nearby_connections_, DisconnectFromEndpoint(_, _, _))
          .Times(0);
      mock_timer_->Fire();
      disconnect_run_loop.Run();
      return;
    }

    base::RunLoop disconnect_from_endpoint_run_loop;
    ExpectDisconnectFromEndpoint(
        disconnect_from_endpoint_run_loop.QuitClosure());
    mock_timer_->Fire();
    disconnect_from_endpoint_run_loop.Run();
  }

  void ExpectDisconnectFromEndpoint(
      base::OnceClosure on_disconnect_from_endpoint_closure) {
    on_disconnect_from_endpoint_closure_ =
        std::move(on_disconnect_from_endpoint_closure);
    EXPECT_CALL(mock_nearby_connections_, DisconnectFromEndpoint(_, _, _))
        .WillOnce(Invoke(
            [&](const std::string& service_id, const std::string& endpoint_id,
                NearbyConnectionsMojom::DisconnectFromEndpointCallback
                    callback) {
              disconnect_from_endpoint_callback_ = std::move(callback);
              std::move(on_disconnect_from_endpoint_closure_).Run();
            }));
  }

  bool IsTimerRunning() const { return mock_timer_->IsRunning(); }

  mojo::Receiver<mojom::FilePayloadListener>& file_payload_listener() {
    return file_payload_listener_;
  }

  const std::vector<mojom::FileTransferUpdatePtr>& file_transfer_updates() {
    return file_transfer_updates_;
  }

  NearbyConnectionsMojom::RequestConnectionCallback
      request_connection_callback_;
  NearbyConnectionsMojom::AcceptConnectionCallback accept_connection_callback_;
  NearbyConnectionsMojom::DisconnectFromEndpointCallback
      disconnect_from_endpoint_callback_;
  base::HistogramTester histogram_tester_;

 private:
  // mojom::NearbyMessageReceiver:
  void OnMessageReceived(const std::string& message) override {
    received_messages_.push_back(message);
    std::move(on_message_received_closure_).Run();
  }

  // mojom::NearbyConnectionStateListener:
  void OnNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep nearby_connection_step,
      mojom::NearbyConnectionStepResult result) override {
    nearby_connection_step_ = nearby_connection_step;
    nearby_connection_step_result_ = result;
  }

  // mojom::FilePayloadListener:
  void OnFileTransferUpdate(mojom::FileTransferUpdatePtr update) override {
    file_transfer_updates_.push_back(std::move(update));
  }

  void OnConnected() { std::move(on_connected_closure_).Run(); }

  void OnDisconnected() { std::move(on_disconnected_closure_).Run(); }

  base::test::TaskEnvironment task_environment_;
  nearby::MockNearbyConnections mock_nearby_connections_;
  FakeNearbyEndpointFinder fake_endpoint_finder_;

  mojo::Remote<mojom::NearbyMessageSender> message_sender_;
  mojo::Remote<mojom::NearbyFilePayloadHandler> file_payload_handler_;
  mojo::Receiver<mojom::NearbyMessageReceiver> message_receiver_{this};
  mojo::Receiver<mojom::NearbyConnectionStateListener>
      nearby_connection_state_listener_{this};
  mojo::Receiver<mojom::FilePayloadListener> file_payload_listener_{this};

  std::unique_ptr<NearbyConnectionBroker> broker_;

  raw_ptr<base::MockOneShotTimer> mock_timer_ = nullptr;

  base::OnceClosure on_connected_closure_;
  base::OnceClosure on_disconnected_closure_;
  base::OnceClosure on_message_received_closure_;
  base::OnceClosure on_disconnect_from_endpoint_closure_;

  mojo::Remote<ConnectionLifecycleListener> connection_lifecycle_listener_;
  mojo::Remote<PayloadListener> payload_listener_;

  std::vector<std::string> received_messages_;
  std::vector<mojom::FileTransferUpdatePtr> file_transfer_updates_;
  mojom::NearbyConnectionStep nearby_connection_step_;
  mojom::NearbyConnectionStepResult nearby_connection_step_result_;
};

TEST_F(NearbyConnectionBrokerImplTest, SendAndReceive) {
  SetUpFullConnection();
  SendMessage("test1", /*success=*/true);
  SendMessage("test2", /*success=*/true);
  ReceiveMessage("test3");
  ReceiveMessage("test4");
  DisconnectMojoBindings(/*expected_to_disconnect=*/true);
  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();
}

TEST_F(NearbyConnectionBrokerImplTest, FailToDisconnect) {
  SetUpFullConnection();
  DisconnectMojoBindings(/*expected_to_disconnect=*/true);
  InvokeDisconnectedFromEndpointCallback(/*success=*/false);
}

TEST_F(NearbyConnectionBrokerImplTest, FailToDisconnect_Timeout) {
  SetUpFullConnection();
  DisconnectMojoBindings(/*expected_to_disconnect=*/true);
  SimulateTimeout(/*expected_to_disconnect=*/false);
}

TEST_F(NearbyConnectionBrokerImplTest, DisconnectsUnexpectedly) {
  SetUpFullConnection();
  InvokeDisconnectedCallback();
}

TEST_F(NearbyConnectionBrokerImplTest,
       DisconnectAfterReceivingFilePayloadWhenFeatureUnsupported) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPhoneHubCameraRoll);
  SetUpFullConnection();

  base::RunLoop disconnect_from_endpoint_run_loop;
  ExpectDisconnectFromEndpoint(disconnect_from_endpoint_run_loop.QuitClosure());
  base::FilePath path;
  base::CreateTemporaryFile(&path);
  ReceiveFilePayload(/*payload_id=*/1234, path);
  disconnect_from_endpoint_run_loop.Run();

  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();
}

TEST_F(NearbyConnectionBrokerImplTest,
       DisconnectAfterReceivingUnregisteredFilePayload) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPhoneHubCameraRoll);
  SetUpFullConnection();

  base::RunLoop disconnect_from_endpoint_run_loop;
  ExpectDisconnectFromEndpoint(disconnect_from_endpoint_run_loop.QuitClosure());
  base::FilePath path;
  base::CreateTemporaryFile(&path);
  RegisterPayloadFile(/*payload_id=*/1234, path, /*expect_success=*/false);
  ReceiveFilePayload(/*payload_id=*/1234, path);
  disconnect_from_endpoint_run_loop.Run();

  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.OperationResult.RegisterPayloadFiles",
      Status::kError,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.FileAction",
      util::FileAction::kUnexpectedFileReceived,
      /*expected_count=*/1);

  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();
}

TEST_F(NearbyConnectionBrokerImplTest, FileTransferUpdateForRegisteredPayload) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPhoneHubCameraRoll);
  SetUpFullConnection();

  int64_t payload_id = 1234;
  base::FilePath path;
  base::CreateTemporaryFile(&path);
  RegisterPayloadFile(payload_id, path, /*expect_success=*/true);
  ReceiveFilePayload(payload_id, path);
  ReceiveFileTransferUpdate(
      PayloadTransferUpdate::New(payload_id, PayloadStatus::kInProgress,
                                 /*total_bytes=*/1000,
                                 /*bytes_transferred=*/100));
  ReceiveFileTransferUpdate(
      PayloadTransferUpdate::New(payload_id, PayloadStatus::kSuccess,
                                 /*total_bytes=*/1000,
                                 /*bytes_transferred=*/1000));
  file_payload_listener().FlushForTesting();

  EXPECT_EQ(2u, file_transfer_updates().size());
  EXPECT_EQ(file_transfer_updates().at(0),
            mojom::FileTransferUpdate::New(
                payload_id, mojom::FileTransferStatus::kInProgress,
                /*total_bytes=*/1000,
                /*bytes_transferred=*/100));
  EXPECT_EQ(file_transfer_updates().at(1),
            mojom::FileTransferUpdate::New(payload_id,
                                           mojom::FileTransferStatus::kSuccess,
                                           /*total_bytes=*/1000,
                                           /*bytes_transferred=*/1000));
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.OperationResult.RegisterPayloadFiles",
      Status::kSuccess,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.FileAction",
      util::FileAction::kRegisteredFileReceived,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.FileTransferResult",
      util::FileTransferResult::kFileTransferSuccess,
      /*expected_count=*/1);

  DisconnectMojoBindings(/*expected_to_disconnect=*/true);
  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();
}

TEST_F(NearbyConnectionBrokerImplTest, FileTransferUpdateForCompletedPayload) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPhoneHubCameraRoll);
  SetUpFullConnection();

  int64_t payload_id = 1234;
  base::FilePath path;
  base::CreateTemporaryFile(&path);
  RegisterPayloadFile(payload_id, path, /*expect_success=*/true);
  ReceiveFilePayload(payload_id, path);
  ReceiveFileTransferUpdate(
      PayloadTransferUpdate::New(payload_id, PayloadStatus::kFailure,
                                 /*total_bytes=*/1000,
                                 /*bytes_transferred=*/100));
  // This is not supposed to trigger a FileTransferUpdate callback as this
  // payload has already been completed and is now untracked.
  ReceiveFileTransferUpdate(
      PayloadTransferUpdate::New(payload_id, PayloadStatus::kInProgress,
                                 /*total_bytes=*/1000,
                                 /*bytes_transferred=*/200));
  file_payload_listener().FlushForTesting();

  EXPECT_EQ(1u, file_transfer_updates().size());
  EXPECT_EQ(file_transfer_updates().at(0),
            mojom::FileTransferUpdate::New(payload_id,
                                           mojom::FileTransferStatus::kFailure,
                                           /*total_bytes=*/1000,
                                           /*bytes_transferred=*/100));
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.FileTransferResult",
      util::FileTransferResult::kFileTransferFailure,
      /*expected_count=*/1);

  DisconnectMojoBindings(/*expected_to_disconnect=*/true);
  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();
}

TEST_F(NearbyConnectionBrokerImplTest,
       FileTransferUpdateForUnregisteredPayload) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPhoneHubCameraRoll);
  SetUpFullConnection();

  base::FilePath path;
  base::CreateTemporaryFile(&path);
  RegisterPayloadFile(/*payload_id=*/1234, path, /*expect_success=*/true);
  ReceiveFileTransferUpdate(PayloadTransferUpdate::New(
      /*payload_id=*/5678, PayloadStatus::kInProgress,
      /*total_bytes=*/1000,
      /*bytes_transferred=*/100));
  file_payload_listener().FlushForTesting();

  EXPECT_TRUE(file_transfer_updates().empty());

  DisconnectMojoBindings(/*expected_to_disconnect=*/true);
  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();
}

TEST_F(NearbyConnectionBrokerImplTest, FileTransferCanceledOnDisconnect) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPhoneHubCameraRoll);
  SetUpFullConnection();

  int64_t payload_id = 1234;
  base::FilePath path;
  base::CreateTemporaryFile(&path);
  RegisterPayloadFile(payload_id, path, /*expect_success=*/true);
  ReceiveFilePayload(payload_id, path);
  ReceiveFileTransferUpdate(
      PayloadTransferUpdate::New(payload_id, PayloadStatus::kInProgress,
                                 /*total_bytes=*/1000,
                                 /*bytes_transferred=*/100));
  // Disconnect before the transfer is complete.
  InvokeDisconnectedCallback();
  file_payload_listener().FlushForTesting();

  EXPECT_EQ(2u, file_transfer_updates().size());
  EXPECT_EQ(file_transfer_updates().at(0),
            mojom::FileTransferUpdate::New(
                payload_id, mojom::FileTransferStatus::kInProgress,
                /*total_bytes=*/1000,
                /*bytes_transferred=*/100));
  EXPECT_EQ(file_transfer_updates().at(1),
            mojom::FileTransferUpdate::New(payload_id,
                                           mojom::FileTransferStatus::kCanceled,
                                           /*total_bytes=*/0,
                                           /*bytes_transferred=*/0));
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.FileTransferResult",
      util::FileTransferResult::kFileTransferCanceled,
      /*expected_count=*/1);
}

TEST_F(NearbyConnectionBrokerImplTest, FileTransferCanceledOnMojoDisconnect) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPhoneHubCameraRoll);
  SetUpFullConnection();

  int64_t payload_id = 1234;
  base::FilePath path;
  base::CreateTemporaryFile(&path);
  RegisterPayloadFile(payload_id, path, /*expect_success=*/true);
  ReceiveFilePayload(payload_id, path);
  ReceiveFileTransferUpdate(
      PayloadTransferUpdate::New(payload_id, PayloadStatus::kInProgress,
                                 /*total_bytes=*/1000,
                                 /*bytes_transferred=*/100));
  // Disconnect before the transfer is complete.
  DisconnectMojoBindings(/*expected_to_disconnect=*/true);
  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();
  file_payload_listener().FlushForTesting();

  EXPECT_EQ(2u, file_transfer_updates().size());
  EXPECT_EQ(file_transfer_updates().at(0),
            mojom::FileTransferUpdate::New(
                payload_id, mojom::FileTransferStatus::kInProgress,
                /*total_bytes=*/1000,
                /*bytes_transferred=*/100));
  EXPECT_EQ(file_transfer_updates().at(1),
            mojom::FileTransferUpdate::New(payload_id,
                                           mojom::FileTransferStatus::kCanceled,
                                           /*total_bytes=*/0,
                                           /*bytes_transferred=*/0));
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.FileTransferResult",
      util::FileTransferResult::kFileTransferCanceled,
      /*expected_count=*/1);
}

TEST_F(NearbyConnectionBrokerImplTest, FailToSend) {
  SetUpFullConnection();
  SendMessage("test", /*success=*/false);
  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();
}

TEST_F(NearbyConnectionBrokerImplTest, FailDiscovery) {
  FailDiscovery();
}

TEST_F(NearbyConnectionBrokerImplTest, FailDiscovery_Timeout) {
  SimulateTimeout(/*expected_to_disconnect=*/false);
}

TEST_F(NearbyConnectionBrokerImplTest, MojoDisconnectionBeforeDiscovery) {
  DisconnectMojoBindings(/*expected_to_disconnect=*/false);
}

TEST_F(NearbyConnectionBrokerImplTest, MojoDisconnectionAfterDiscovery) {
  DiscoverEndpoint();
  DisconnectMojoBindings(/*expected_to_disconnect=*/true);

  // Run callback to prevent DCHECK() crash that ensures all Mojo callbacks are
  // invoked.
  std::move(request_connection_callback_).Run(Status::kError);
}

TEST_F(NearbyConnectionBrokerImplTest,
       FailRequestingConnection_AlreadyConnectedToEndpoint) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kAlreadyConnectedToEndpoint);
}

TEST_F(NearbyConnectionBrokerImplTest,
       FailRequestingConnection_PayloadUnknown) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kPayloadUnknown);
}

TEST_F(NearbyConnectionBrokerImplTest,
       FailRequestingConnection_NotConnectedToEndpoint) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kNotConnectedToEndpoint);
}

TEST_F(NearbyConnectionBrokerImplTest,
       FailRequestingConnection_AlreadyAdvertising) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kAlreadyAdvertising);
}

TEST_F(NearbyConnectionBrokerImplTest,
       FailRequestingConnection_AlreadyHaveActiveStrategy) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kAlreadyHaveActiveStrategy);
}

TEST_F(NearbyConnectionBrokerImplTest,
       FailRequestingConnection_AlreadyListening) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kAlreadyListening);
}

TEST_F(NearbyConnectionBrokerImplTest, FailRequestingConnection_Unknown) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kUnknown);
}

TEST_F(NearbyConnectionBrokerImplTest,
       FailRequestingConnection_EndpointIOError) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kEndpointIOError);
}

TEST_F(NearbyConnectionBrokerImplTest,
       FailRequestingConnection_EndpointUnknown) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kEndpointUnknown);
}

TEST_F(NearbyConnectionBrokerImplTest, FailRequestingConnection_BleError) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kBleError);
}

TEST_F(NearbyConnectionBrokerImplTest,
       FailRequestingConnection_BluetoothError) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kBluetoothError);
}

TEST_F(NearbyConnectionBrokerImplTest,
       FailRequestingConnection_OutOfOrderApiCall) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kOutOfOrderApiCall);
}

TEST_F(NearbyConnectionBrokerImplTest, FailRequestingConnection_WifiLanError) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kWifiLanError);
}

TEST_F(NearbyConnectionBrokerImplTest, FailRequestingConnection_Reset) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kReset);
}

TEST_F(NearbyConnectionBrokerImplTest,
       FailRequestingConnection_NearbyConnectionTimeout) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kTimeout);
}

TEST_F(NearbyConnectionBrokerImplTest, FailRequestingConnection_Timeout) {
  DiscoverEndpoint();
  SimulateTimeout(/*expected_to_disconnect=*/true);
  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();
}

TEST_F(NearbyConnectionBrokerImplTest,
       MojoDisconnectionAfterRequestConnection) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kSuccess);
  NotifyConnectionInitiated();
  DisconnectMojoBindings(/*expected_to_disconnect=*/true);
  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();

  // Run callback to prevent DCHECK() crash that ensures all Mojo callbacks are
  // invoked.
  std::move(accept_connection_callback_).Run(Status::kError);
}

TEST_F(NearbyConnectionBrokerImplTest, FailAcceptingConnection) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kSuccess);
  NotifyConnectionInitiated();
  InvokeAcceptConnectionCallback(Status::kConnectionRejected);
  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();
}

// Regression test for https://crbug.com/1175489.
TEST_F(NearbyConnectionBrokerImplTest, OnAcceptedBeforeAcceptCallback) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kSuccess);
  NotifyConnectionInitiated();

  // Invoke OnConnectionAccepted() callback before the AcceptConnection()
  // call returns a value. Generally we expect AcceptConnection() to return
  // before OnConnectionAccepted(), but we've seen in practice that this is
  // sometimes not the case.
  NotifyConnectionAccepted();
  InvokeAcceptConnectionCallback(Status::kSuccess);

  // The connection is now considered complete, so there should be no further
  // timeout.
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(NearbyConnectionBrokerImplTest, FailAcceptingConnection_Timeout) {
  DiscoverEndpoint();
  InvokeRequestConnectionCallback(Status::kSuccess);
  NotifyConnectionInitiated();
  SimulateTimeout(/*expected_to_disconnect=*/true);
  InvokeDisconnectedFromEndpointCallback(/*success=*/true);
  InvokeDisconnectedCallback();
}

}  // namespace secure_channel
}  // namespace ash

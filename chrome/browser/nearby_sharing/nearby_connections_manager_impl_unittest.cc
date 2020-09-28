// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_connections_manager_impl.h"

#include <algorithm>
#include <memory>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/nearby_sharing/constants.h"
#include "chrome/browser/nearby_sharing/mock_nearby_connections.h"
#include "chrome/browser/nearby_sharing/mock_nearby_process_manager.h"
#include "chrome/browser/nearby_sharing/nearby_connection_impl.h"
#include "chrome/services/sharing/public/mojom/nearby_connections_types.mojom.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kServiceId[] = "NearbySharing";
const location::nearby::connections::mojom::Strategy kStrategy =
    location::nearby::connections::mojom::Strategy::kP2pPointToPoint;
const char kEndpointId[] = "endpoint_id";
const char kRemoteEndpointId[] = "remote_endpoint_id";
const char kEndpointInfo[] = {0x0d, 0x07, 0x07, 0x07, 0x07};
const char kRemoteEndpointInfo[] = {0x0d, 0x07, 0x06, 0x08, 0x09};
const char kAuthenticationToken[] = "authentication_token";
const char kRawAuthenticationToken[] = {0x00, 0x05, 0x04, 0x03, 0x02};
const char kBytePayload[] = {0x08, 0x09, 0x06, 0x04, 0x0f};
const char kBytePayload2[] = {0x0a, 0x0b, 0x0c, 0x0d, 0x0e};
const int64_t kPayloadId = 689777;
const int64_t kPayloadId2 = 777689;
const uint64_t kTotalSize = 5201314;
const uint64_t kBytesTransferred = 721831;
const uint8_t kPayload[] = {0x0f, 0x0a, 0x0c, 0x0e};
const char kBluetoothMacAddress[] = {0x00, 0x00, 0xe6, 0x88, 0x64, 0x13};
const char kInvalidBluetoothMacAddress[] = {0x07, 0x07, 0x07};

void VerifyFileReadWrite(base::File& input_file, base::File& output_file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const std::vector<uint8_t> expected_bytes(std::begin(kPayload),
                                            std::end(kPayload));
  EXPECT_TRUE(output_file.WriteAndCheck(
      /*offset=*/0, base::make_span(expected_bytes)));
  output_file.Flush();
  output_file.Close();

  std::vector<uint8_t> payload_bytes(input_file.GetLength());
  EXPECT_TRUE(input_file.ReadAndCheck(
      /*offset=*/0, base::make_span(payload_bytes)));
  EXPECT_EQ(expected_bytes, payload_bytes);
  input_file.Close();
}

base::FilePath InitializeTemporaryFile(base::File& file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path;
  if (!base::CreateTemporaryFile(&path))
    return path;

  file.Initialize(path, base::File::Flags::FLAG_CREATE_ALWAYS |
                            base::File::Flags::FLAG_READ |
                            base::File::Flags::FLAG_WRITE);
  EXPECT_TRUE(file.WriteAndCheck(
      /*offset=*/0, base::make_span(kPayload, sizeof(kPayload))));
  EXPECT_TRUE(file.Flush());
  return path;
}

}  // namespace

using Status = location::nearby::connections::mojom::Status;
using DiscoveredEndpointInfo =
    location::nearby::connections::mojom::DiscoveredEndpointInfo;
using ConnectionInfo = location::nearby::connections::mojom::ConnectionInfo;
using MediumSelection = location::nearby::connections::mojom::MediumSelection;
using PayloadContent = location::nearby::connections::mojom::PayloadContent;
using PayloadStatus = location::nearby::connections::mojom::PayloadStatus;
using PayloadTransferUpdate =
    location::nearby::connections::mojom::PayloadTransferUpdate;
using Payload = location::nearby::connections::mojom::Payload;
using BytesPayload = location::nearby::connections::mojom::BytesPayload;
using FilePayload = location::nearby::connections::mojom::FilePayload;

class MockDiscoveryListener
    : public NearbyConnectionsManager::DiscoveryListener {
 public:
  MOCK_METHOD(void,
              OnEndpointDiscovered,
              (const std::string& endpoint_id,
               const std::vector<uint8_t>& endpoint_info),
              (override));
  MOCK_METHOD(void,
              OnEndpointLost,
              (const std::string& endpoint_id),
              (override));
};

class MockIncomingConnectionListener
    : public NearbyConnectionsManager::IncomingConnectionListener {
 public:
  MOCK_METHOD(void,
              OnIncomingConnection,
              (const std::string& endpoint_id,
               const std::vector<uint8_t>& endpoint_info,
               NearbyConnection* connection),
              (override));
};

class MockPayloadStatusListener
    : public NearbyConnectionsManager::PayloadStatusListener {
 public:
  MOCK_METHOD(void,
              OnStatusUpdate,
              (PayloadTransferUpdatePtr update),
              (override));
};

class NearbyConnectionsManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_CALL(nearby_process_manager_,
                GetOrStartNearbyConnections(testing::Eq(&profile_)))
        .WillRepeatedly(testing::Return(&nearby_connections_));
  }

 protected:
  void StartDiscovery(
      mojo::Remote<EndpointDiscoveryListener>& listener_remote,
      testing::NiceMock<MockDiscoveryListener>& discovery_listener) {
    EXPECT_CALL(nearby_connections_, StartDiscovery)
        .WillOnce([&listener_remote](
                      const std::string& service_id,
                      DiscoveryOptionsPtr options,
                      mojo::PendingRemote<EndpointDiscoveryListener> listener,
                      NearbyConnectionsMojom::StartDiscoveryCallback callback) {
          EXPECT_EQ(kServiceId, service_id);
          EXPECT_EQ(kStrategy, options->strategy);

          listener_remote.Bind(std::move(listener));
          std::move(callback).Run(Status::kSuccess);
        });
    base::MockCallback<NearbyConnectionsManager::ConnectionsCallback> callback;
    EXPECT_CALL(callback, Run(testing::Eq(Status::kSuccess)));
    nearby_connections_manager_.StartDiscovery(&discovery_listener,
                                               callback.Get());
  }

  void StartAdvertising(
      mojo::Remote<ConnectionLifecycleListener>& listener_remote,
      testing::NiceMock<MockIncomingConnectionListener>&
          incoming_connection_listener) {
    const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                   std::end(kEndpointInfo));
    EXPECT_CALL(nearby_connections_, StartAdvertising)
        .WillOnce(
            [&](const std::vector<uint8_t>& endpoint_info,
                const std::string& service_id, AdvertisingOptionsPtr options,
                mojo::PendingRemote<ConnectionLifecycleListener> listener,
                NearbyConnectionsMojom::StartAdvertisingCallback callback) {
              EXPECT_EQ(local_endpoint_info, endpoint_info);
              EXPECT_EQ(kServiceId, service_id);
              EXPECT_EQ(kStrategy, options->strategy);
              EXPECT_TRUE(options->enforce_topology_constraints);

              listener_remote.Bind(std::move(listener));
              std::move(callback).Run(Status::kSuccess);
            });
    base::MockCallback<NearbyConnectionsManager::ConnectionsCallback> callback;
    EXPECT_CALL(callback, Run(testing::Eq(Status::kSuccess)));
    nearby_connections_manager_.StartAdvertising(
        local_endpoint_info, &incoming_connection_listener,
        PowerLevel::kHighPower, DataUsage::kOnline, callback.Get());
  }

  enum class ConnectionResponse { kAccepted, kRejceted, kDisconnected };

  NearbyConnection* Connect(
      mojo::Remote<ConnectionLifecycleListener>& connection_listener_remote,
      mojo::Remote<PayloadListener>& payload_listener_remote,
      ConnectionResponse connection_response) {
    const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                   std::end(kEndpointInfo));
    const std::vector<uint8_t> remote_endpoint_info(
        std::begin(kRemoteEndpointInfo), std::end(kRemoteEndpointInfo));
    const std::vector<uint8_t> raw_authentication_token(
        std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

    EXPECT_CALL(nearby_connections_, RequestConnection)
        .WillOnce(
            [&](const std::vector<uint8_t>& endpoint_info,
                const std::string& endpoint_id, ConnectionOptionsPtr options,
                mojo::PendingRemote<ConnectionLifecycleListener> listener,
                NearbyConnectionsMojom::RequestConnectionCallback callback) {
              EXPECT_EQ(local_endpoint_info, endpoint_info);
              EXPECT_EQ(kRemoteEndpointId, endpoint_id);

              connection_listener_remote.Bind(std::move(listener));
              std::move(callback).Run(Status::kSuccess);
            });

    base::RunLoop run_loop;
    NearbyConnection* nearby_connection;
    nearby_connections_manager_.Connect(
        local_endpoint_info, kRemoteEndpointId,
        /*bluetooth_mac_address=*/base::nullopt, DataUsage::kOffline,
        base::BindLambdaForTesting([&](NearbyConnection* connection) {
          nearby_connection = connection;
          run_loop.Quit();
        }));

    base::RunLoop accept_run_loop;
    EXPECT_CALL(nearby_connections_, AcceptConnection)
        .WillOnce(
            [&](const std::string& endpoint_id,
                mojo::PendingRemote<PayloadListener> listener,
                NearbyConnectionsMojom::AcceptConnectionCallback callback) {
              EXPECT_EQ(kRemoteEndpointId, endpoint_id);

              payload_listener_remote.Bind(std::move(listener));
              std::move(callback).Run(Status::kSuccess);
              accept_run_loop.Quit();
            });

    connection_listener_remote->OnConnectionInitiated(
        kRemoteEndpointId,
        ConnectionInfo::New(kAuthenticationToken, raw_authentication_token,
                            remote_endpoint_info,
                            /*is_incoming_connection=*/false));
    accept_run_loop.Run();

    switch (connection_response) {
      case ConnectionResponse::kAccepted:
        connection_listener_remote->OnConnectionAccepted(kRemoteEndpointId);
        break;
      case ConnectionResponse::kRejceted:
        connection_listener_remote->OnConnectionRejected(
            kRemoteEndpointId, Status::kConnectionRejected);
        break;
      case ConnectionResponse::kDisconnected:
        connection_listener_remote->OnDisconnected(kRemoteEndpointId);
        break;
    }
    run_loop.Run();

    return nearby_connection;
  }

  NearbyConnection* OnIncomingConnection(
      mojo::Remote<ConnectionLifecycleListener>& connection_listener_remote,
      testing::NiceMock<MockIncomingConnectionListener>&
          incoming_connection_listener,
      mojo::Remote<PayloadListener>& payload_listener_remote) {
    base::RunLoop accept_run_loop;
    EXPECT_CALL(nearby_connections_, AcceptConnection)
        .WillOnce(
            [&](const std::string& endpoint_id,
                mojo::PendingRemote<PayloadListener> listener,
                NearbyConnectionsMojom::AcceptConnectionCallback callback) {
              EXPECT_EQ(kRemoteEndpointId, endpoint_id);

              payload_listener_remote.Bind(std::move(listener));
              std::move(callback).Run(Status::kSuccess);
              accept_run_loop.Quit();
            });

    const std::vector<uint8_t> remote_endpoint_info(
        std::begin(kRemoteEndpointInfo), std::end(kRemoteEndpointInfo));
    const std::vector<uint8_t> raw_authentication_token(
        std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

    connection_listener_remote->OnConnectionInitiated(
        kRemoteEndpointId,
        ConnectionInfo::New(kAuthenticationToken, raw_authentication_token,
                            remote_endpoint_info,
                            /*is_incoming_connection=*/true));
    accept_run_loop.Run();

    NearbyConnection* nearby_connection;
    base::RunLoop incoming_connection_run_loop;
    EXPECT_CALL(incoming_connection_listener,
                OnIncomingConnection(testing::_, testing::_, testing::_))
        .WillOnce([&](const std::string&, const std::vector<uint8_t>&,
                      NearbyConnection* connection) {
          nearby_connection = connection;
          incoming_connection_run_loop.Quit();
        });

    connection_listener_remote->OnConnectionAccepted(kRemoteEndpointId);
    incoming_connection_run_loop.Run();

    EXPECT_EQ(raw_authentication_token,
              nearby_connections_manager_.GetRawAuthenticationToken(
                  kRemoteEndpointId));

    return nearby_connection;
  }

  void SendPayload(
      testing::NiceMock<MockPayloadStatusListener>& payload_listener) {
    const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                                std::end(kPayload));

    base::File file;
    InitializeTemporaryFile(file);

    base::RunLoop run_loop;
    EXPECT_CALL(nearby_connections_, SendPayload)
        .WillOnce([&](const std::vector<std::string>& endpoint_ids,
                      PayloadPtr payload,
                      NearbyConnectionsMojom::SendPayloadCallback callback) {
          ASSERT_EQ(1u, endpoint_ids.size());
          EXPECT_EQ(kRemoteEndpointId, endpoint_ids.front());
          ASSERT_TRUE(payload);
          ASSERT_EQ(PayloadContent::Tag::FILE, payload->content->which());

          base::ScopedAllowBlockingForTesting allow_blocking;
          base::File file = std::move(payload->content->get_file()->file);
          std::vector<uint8_t> payload_bytes(file.GetLength());
          EXPECT_TRUE(file.ReadAndCheck(
              /*offset=*/0, base::make_span(payload_bytes)));
          EXPECT_EQ(expected_payload, payload_bytes);

          std::move(callback).Run(Status::kSuccess);
          run_loop.Quit();
        });

    nearby_connections_manager_.Send(
        kRemoteEndpointId,
        Payload::New(kPayloadId, PayloadContent::NewFile(
                                     FilePayload::New(std::move(file)))),
        &payload_listener);
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier> network_notifier_ =
      net::test::MockNetworkChangeNotifier::Create();
  base::ScopedDisallowBlocking disallow_blocking_;
  testing::NiceMock<MockNearbyConnections> nearby_connections_;
  testing::NiceMock<MockNearbyProcessManager> nearby_process_manager_;
  NearbyConnectionsManagerImpl nearby_connections_manager_{
      &nearby_process_manager_, &profile_};
};

TEST_F(NearbyConnectionsManagerImplTest, DiscoveryFlow) {
  const std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                           std::end(kEndpointInfo));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(listener_remote, discovery_listener);

  // Invoking OnEndpointFound over remote will invoke OnEndpointDiscovered.
  base::RunLoop discovered_run_loop;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce([&discovered_run_loop]() { discovered_run_loop.Quit(); });
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
  discovered_run_loop.Run();

  // Invoking OnEndpointFound over remote on same endpointId will do nothing.
  EXPECT_CALL(discovery_listener, OnEndpointDiscovered(testing::_, testing::_))
      .Times(0);
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));

  // Invoking OnEndpointLost over remote will invoke OnEndpointLost.
  base::RunLoop lost_run_loop;
  EXPECT_CALL(discovery_listener, OnEndpointLost(testing::Eq(kEndpointId)))
      .WillOnce([&lost_run_loop]() { lost_run_loop.Quit(); });
  listener_remote->OnEndpointLost(kEndpointId);
  lost_run_loop.Run();

  // Invoking OnEndpointLost over remote on same endpointId will do nothing.
  EXPECT_CALL(discovery_listener, OnEndpointLost(testing::_)).Times(0);
  listener_remote->OnEndpointLost(kEndpointId);

  // After OnEndpointLost the same endpotinId can be discovered again.
  base::RunLoop discovered_run_loop_2;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce([&discovered_run_loop_2]() { discovered_run_loop_2.Quit(); });
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
  discovered_run_loop_2.Run();

  // Stop discvoery will call through mojo.
  EXPECT_CALL(nearby_connections_, StopDiscovery).Times(1);
  nearby_connections_manager_.StopDiscovery();

  // StartDiscovery again will succeed.
  listener_remote.reset();
  StartDiscovery(listener_remote, discovery_listener);

  // Same endpotinId can be discovered again.
  base::RunLoop discovered_run_loop_3;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce([&discovered_run_loop_3]() { discovered_run_loop_3.Quit(); });
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
  discovered_run_loop_3.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, DiscoveryProcessStopped) {
  const std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                           std::end(kEndpointInfo));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(listener_remote, discovery_listener);

  EXPECT_CALL(nearby_connections_, StopAllEndpoints).Times(0);
  nearby_connections_manager_.OnNearbyProcessStopped();

  // Invoking OnEndpointFound will do nothing.
  EXPECT_CALL(discovery_listener, OnEndpointDiscovered(testing::_, testing::_))
      .Times(0);
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
}

TEST_F(NearbyConnectionsManagerImplTest, StopDiscoveryBeforeStart) {
  EXPECT_CALL(nearby_connections_, StopDiscovery).Times(0);
  nearby_connections_manager_.StopDiscovery();
}

using ConnectionMediumsTestParam =
    std::tuple<DataUsage, net::NetworkChangeNotifier::ConnectionType>;
class NearbyConnectionsManagerImplTestConnectionMediums
    : public NearbyConnectionsManagerImplTest,
      public testing::WithParamInterface<ConnectionMediumsTestParam> {};

TEST_P(NearbyConnectionsManagerImplTestConnectionMediums,
       RequestConnection_MediumSelection) {
  const ConnectionMediumsTestParam& param = GetParam();
  DataUsage data_usage = std::get<0>(param);
  net::NetworkChangeNotifier::ConnectionType connection_type =
      std::get<1>(param);

  network_notifier_->SetConnectionType(connection_type);
  bool should_use_web_rtc =
      data_usage != DataUsage::kOffline &&
      connection_type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      (data_usage != DataUsage::kWifiOnly ||
       !net::NetworkChangeNotifier::IsConnectionCellular(connection_type));

  // TODO(crbug.com/1129069): Update when WiFi LAN is supported.
  auto expected_mediums = MediumSelection::New(
      /*bluetooth=*/true,
      /*ble=*/false,
      /*web_rtc=*/should_use_web_rtc,
      /*wifi_lan=*/false);

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  EXPECT_CALL(nearby_connections_, RequestConnection)
      .WillOnce(
          [&](const std::vector<uint8_t>& endpoint_info,
              const std::string& endpoint_id, ConnectionOptionsPtr options,
              mojo::PendingRemote<ConnectionLifecycleListener> listener,
              NearbyConnectionsMojom::RequestConnectionCallback callback) {
            EXPECT_EQ(local_endpoint_info, endpoint_info);
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            EXPECT_EQ(expected_mediums, options->allowed_mediums);
            std::move(callback).Run(Status::kSuccess);
          });

  nearby_connections_manager_.Connect(local_endpoint_info, kRemoteEndpointId,
                                      /*bluetooth_mac_address=*/base::nullopt,
                                      data_usage, base::DoNothing());
}

INSTANTIATE_TEST_SUITE_P(
    NearbyConnectionsManagerImplTestConnectionMediums,
    NearbyConnectionsManagerImplTestConnectionMediums,
    testing::Combine(
        testing::Values(DataUsage::kWifiOnly,
                        DataUsage::kOffline,
                        DataUsage::kOnline),
        testing::Values(net::NetworkChangeNotifier::CONNECTION_NONE,
                        net::NetworkChangeNotifier::CONNECTION_WIFI,
                        net::NetworkChangeNotifier::CONNECTION_3G)));

struct ConnectionBluetoothMacAddressTestData {
  base::Optional<std::vector<uint8_t>> bluetooth_mac_address;
  base::Optional<std::vector<uint8_t>> expected_bluetooth_mac_address;
} kConnectionBluetoothMacAddressTestData[] = {
    {base::make_optional(std::vector<uint8_t>(std::begin(kBluetoothMacAddress),
                                              std::end(kBluetoothMacAddress))),
     base::make_optional(std::vector<uint8_t>(std::begin(kBluetoothMacAddress),
                                              std::end(kBluetoothMacAddress)))},
    {base::make_optional(
         std::vector<uint8_t>(std::begin(kInvalidBluetoothMacAddress),
                              std::end(kInvalidBluetoothMacAddress))),
     base::nullopt},
    {base::nullopt, base::nullopt}};

class NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress
    : public NearbyConnectionsManagerImplTest,
      public testing::WithParamInterface<
          ConnectionBluetoothMacAddressTestData> {};

TEST_P(NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress,
       RequestConnection_BluetoothMacAddress) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  EXPECT_CALL(nearby_connections_, RequestConnection)
      .WillOnce(
          [&](const std::vector<uint8_t>& endpoint_info,
              const std::string& endpoint_id, ConnectionOptionsPtr options,
              mojo::PendingRemote<ConnectionLifecycleListener> listener,
              NearbyConnectionsMojom::RequestConnectionCallback callback) {
            EXPECT_EQ(local_endpoint_info, endpoint_info);
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            EXPECT_EQ(GetParam().expected_bluetooth_mac_address,
                      options->remote_bluetooth_mac_address);
            std::move(callback).Run(Status::kSuccess);
          });

  nearby_connections_manager_.Connect(local_endpoint_info, kRemoteEndpointId,
                                      GetParam().bluetooth_mac_address,
                                      DataUsage::kOffline, base::DoNothing());
}

INSTANTIATE_TEST_SUITE_P(
    NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress,
    NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress,
    testing::ValuesIn(kConnectionBluetoothMacAddressTestData));

TEST_F(NearbyConnectionsManagerImplTest, ConnectRejected) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kRejceted);
  EXPECT_FALSE(nearby_connection);
  EXPECT_FALSE(
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectDisconnected) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kDisconnected);
  EXPECT_FALSE(nearby_connection);
  EXPECT_FALSE(
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectAccepted) {
  const std::vector<uint8_t> raw_authentication_token(
      std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  EXPECT_TRUE(nearby_connection);
  EXPECT_EQ(
      raw_authentication_token,
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectReadBeforeAppend) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Read before message is appended should also succeed.
  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_EQ(byte_payload, bytes);
        read_run_loop.Quit();
      }));

  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId,
                   PayloadContent::NewBytes(BytesPayload::New(byte_payload))));
  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  read_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectReadAfterAppend) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));
  const std::vector<uint8_t> byte_payload_2(std::begin(kBytePayload2),
                                            std::end(kBytePayload2));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Read after message is appended should succeed.
  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId,
                   PayloadContent::NewBytes(BytesPayload::New(byte_payload))));
  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId2, PayloadContent::NewBytes(
                                    BytesPayload::New(byte_payload_2))));
  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId2, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));

  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_EQ(byte_payload, bytes);
        read_run_loop.Quit();
      }));
  read_run_loop.Run();

  base::RunLoop read_run_loop_2;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_EQ(byte_payload_2, bytes);
        read_run_loop_2.Quit();
      }));
  read_run_loop_2.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectWrite) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  base::RunLoop run_loop;
  EXPECT_CALL(nearby_connections_, SendPayload)
      .WillOnce([&](const std::vector<std::string>& endpoint_ids,
                    PayloadPtr payload,
                    NearbyConnectionsMojom::SendPayloadCallback callback) {
        ASSERT_EQ(1u, endpoint_ids.size());
        EXPECT_EQ(kRemoteEndpointId, endpoint_ids.front());
        ASSERT_TRUE(payload);
        ASSERT_EQ(PayloadContent::Tag::BYTES, payload->content->which());
        EXPECT_EQ(byte_payload, payload->content->get_bytes()->bytes);

        std::move(callback).Run(Status::kSuccess);
        run_loop.Quit();
      });

  nearby_connection->Write(byte_payload);
  run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosed) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Close should invoke disconnection callback and read callback.
  base::RunLoop close_run_loop;
  nearby_connection->SetDisconnectionListener(
      base::BindLambdaForTesting([&]() { close_run_loop.Quit(); }));
  base::RunLoop read_run_loop_3;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_FALSE(bytes);
        read_run_loop_3.Quit();
      }));

  EXPECT_CALL(nearby_connections_, DisconnectFromEndpoint)
      .WillOnce(
          [&](const std::string& endpoint_id,
              NearbyConnectionsMojom::DisconnectFromEndpointCallback callback) {
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            std::move(callback).Run(Status::kSuccess);
          });
  nearby_connection->Close();
  close_run_loop.Run();
  read_run_loop_3.Run();

  EXPECT_FALSE(
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosedByRemote) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Remote closing should invoke disconnection callback and read callback.
  base::RunLoop close_run_loop;
  nearby_connection->SetDisconnectionListener(
      base::BindLambdaForTesting([&]() { close_run_loop.Quit(); }));
  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_FALSE(bytes);
        read_run_loop.Quit();
      }));

  connection_listener_remote->OnDisconnected(kRemoteEndpointId);
  close_run_loop.Run();
  read_run_loop.Run();

  EXPECT_FALSE(
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosedByClient) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Remote closing should invoke disconnection callback and read callback.
  base::RunLoop close_run_loop;
  nearby_connection->SetDisconnectionListener(
      base::BindLambdaForTesting([&]() { close_run_loop.Quit(); }));
  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_FALSE(bytes);
        read_run_loop.Quit();
      }));

  EXPECT_CALL(nearby_connections_, DisconnectFromEndpoint)
      .WillOnce(
          [&](const std::string& endpoint_id,
              NearbyConnectionsMojom::DisconnectFromEndpointCallback callback) {
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            std::move(callback).Run(Status::kSuccess);
          });
  nearby_connections_manager_.Disconnect(kRemoteEndpointId);
  close_run_loop.Run();
  read_run_loop.Run();

  EXPECT_FALSE(
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectSendPayload) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  Connect(connection_listener_remote, payload_listener_remote,
          ConnectionResponse::kAccepted);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  SendPayload(payload_listener);

  auto expected_update = PayloadTransferUpdate::New(
      kPayloadId, PayloadStatus::kInProgress, kTotalSize, kBytesTransferred);
  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_))
      .WillOnce(
          [&](MockPayloadStatusListener::PayloadTransferUpdatePtr update) {
            EXPECT_EQ(expected_update, update);
            payload_run_loop.Quit();
          });

  payload_listener_remote->OnPayloadTransferUpdate(kRemoteEndpointId,
                                                   expected_update.Clone());
  payload_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectCancelPayload) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  Connect(connection_listener_remote, payload_listener_remote,
          ConnectionResponse::kAccepted);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  SendPayload(payload_listener);

  base::RunLoop cancel_run_loop;
  EXPECT_CALL(nearby_connections_, CancelPayload)
      .WillOnce([&](int64_t payload_id,
                    NearbyConnectionsMojom::CancelPayloadCallback callback) {
        EXPECT_EQ(kPayloadId, payload_id);

        std::move(callback).Run(Status::kSuccess);
        cancel_run_loop.Quit();
      });

  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_))
      .WillOnce(
          [&](MockPayloadStatusListener::PayloadTransferUpdatePtr update) {
            EXPECT_EQ(kPayloadId, update->payload_id);
            EXPECT_EQ(PayloadStatus::kCanceled, update->status);
            EXPECT_EQ(0u, update->total_bytes);
            EXPECT_EQ(0u, update->bytes_transferred);
            payload_run_loop.Quit();
          });

  nearby_connections_manager_.Cancel(kPayloadId);
  payload_run_loop.Run();
  cancel_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectTimeout) {
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will time out.
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));

  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  NearbyConnectionsMojom::RequestConnectionCallback connect_callback;
  EXPECT_CALL(nearby_connections_, RequestConnection)
      .WillOnce(
          [&](const std::vector<uint8_t>& endpoint_info,
              const std::string& endpoint_id, ConnectionOptionsPtr options,
              mojo::PendingRemote<ConnectionLifecycleListener> listener,
              NearbyConnectionsMojom::RequestConnectionCallback callback) {
            EXPECT_EQ(local_endpoint_info, endpoint_info);
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);

            connection_listener_remote.Bind(std::move(listener));
            // Do not call callback until connection timed out.
            connect_callback = std::move(callback);
          });

  // Timing out should call disconnect.
  EXPECT_CALL(nearby_connections_, DisconnectFromEndpoint)
      .WillOnce(
          [&](const std::string& endpoint_id,
              NearbyConnectionsMojom::DisconnectFromEndpointCallback callback) {
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            std::move(callback).Run(Status::kSuccess);
          });

  base::RunLoop run_loop;
  NearbyConnection* nearby_connection = nullptr;
  nearby_connections_manager_.Connect(
      local_endpoint_info, kRemoteEndpointId,
      /*bluetooth_mac_address=*/base::nullopt, DataUsage::kOffline,
      base::BindLambdaForTesting([&](NearbyConnection* connection) {
        nearby_connection = connection;
        run_loop.Quit();
      }));
  // Simulate time passing until timeout is reached.
  task_environment_.FastForwardBy(kInitiateNearbyConnectionTimeout);
  run_loop.Run();

  // Expect callback to be called with null connection.
  EXPECT_FALSE(nearby_connection);

  // Resolving the connect callback after timeout should do nothing.
  std::move(connect_callback).Run(Status::kSuccess);
}

TEST_F(NearbyConnectionsManagerImplTest, StartAdvertising) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);
}

TEST_F(NearbyConnectionsManagerImplTest, IncomingPayloadStatusListener) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  nearby_connections_manager_.RegisterPayloadStatusListener(kPayloadId,
                                                            &payload_listener);

  auto expected_update = PayloadTransferUpdate::New(
      kPayloadId, PayloadStatus::kInProgress, kTotalSize, kBytesTransferred);
  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_))
      .WillOnce(
          [&](MockPayloadStatusListener::PayloadTransferUpdatePtr update) {
            EXPECT_EQ(expected_update, update);
            payload_run_loop.Quit();
          });

  payload_listener_remote->OnPayloadTransferUpdate(kRemoteEndpointId,
                                                   expected_update.Clone());
  payload_run_loop.Run();

  // After success status.
  base::RunLoop payload_run_loop_2;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_))
      .WillOnce([&payload_run_loop_2]() { payload_run_loop_2.Quit(); });

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_run_loop_2.Run();

  // PayloadStatusListener will be unregistered and won't receive further
  // updates.
  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_)).Times(0);

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
}

TEST_F(NearbyConnectionsManagerImplTest, IncomingRegisterPayloadPath) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  base::RunLoop register_payload_run_loop;
  EXPECT_CALL(nearby_connections_, RegisterPayloadFile)
      .WillOnce(
          [&](int64_t payload_id, base::File input_file, base::File output_file,
              NearbyConnectionsMojom::RegisterPayloadFileCallback callback) {
            EXPECT_EQ(kPayloadId, payload_id);
            ASSERT_TRUE(input_file.IsValid());
            ASSERT_TRUE(output_file.IsValid());
            VerifyFileReadWrite(input_file, output_file);

            std::move(callback).Run(Status::kSuccess);
            register_payload_run_loop.Quit();
          });

  base::FilePath path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CreateTemporaryFile(&path));
  }

  base::MockCallback<NearbyConnectionsManager::ConnectionsCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(Status::kSuccess)));
  nearby_connections_manager_.RegisterPayloadPath(kPayloadId, path,
                                                  callback.Get());
  register_payload_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, IncomingBytesPayload) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  nearby_connections_manager_.RegisterPayloadStatusListener(kPayloadId,
                                                            &payload_listener);

  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId, PayloadContent::NewBytes(
                                   BytesPayload::New(expected_payload))));

  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_))
      .WillOnce([&payload_run_loop]() { payload_run_loop.Quit(); });

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_run_loop.Run();

  Payload* payload = nearby_connections_manager_.GetIncomingPayload(kPayloadId);
  ASSERT_TRUE(payload);
  ASSERT_TRUE(payload->content->is_bytes());
  EXPECT_EQ(expected_payload, payload->content->get_bytes()->bytes);
}

TEST_F(NearbyConnectionsManagerImplTest, IncomingFilePayload) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  nearby_connections_manager_.RegisterPayloadStatusListener(kPayloadId,
                                                            &payload_listener);

  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  base::File file;
  InitializeTemporaryFile(file);

  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId,
                   PayloadContent::NewFile(FilePayload::New(std::move(file)))));

  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_))
      .WillOnce([&payload_run_loop]() { payload_run_loop.Quit(); });

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_run_loop.Run();

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    Payload* payload =
        nearby_connections_manager_.GetIncomingPayload(kPayloadId);
    ASSERT_TRUE(payload);
    ASSERT_TRUE(payload->content->is_file());
    std::vector<uint8_t> payload_bytes(
        payload->content->get_file()->file.GetLength());
    EXPECT_TRUE(payload->content->get_file()->file.ReadAndCheck(
        /*offset=*/0, base::make_span(payload_bytes)));
    EXPECT_EQ(expected_payload, payload_bytes);
  }
}

TEST_F(NearbyConnectionsManagerImplTest, ClearIncomingPayloads) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  nearby_connections_manager_.RegisterPayloadStatusListener(kPayloadId,
                                                            &payload_listener);

  base::File file;
  InitializeTemporaryFile(file);
  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId,
                   PayloadContent::NewFile(FilePayload::New(std::move(file)))));

  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_))
      .WillOnce([&payload_run_loop]() { payload_run_loop.Quit(); });

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_run_loop.Run();

  nearby_connections_manager_.ClearIncomingPayloads();

  EXPECT_FALSE(nearby_connections_manager_.GetIncomingPayload(kPayloadId));
}

using MediumsTestParam = std::
    tuple<PowerLevel, DataUsage, net::NetworkChangeNotifier::ConnectionType>;
class NearbyConnectionsManagerImplTestMediums
    : public NearbyConnectionsManagerImplTest,
      public testing::WithParamInterface<MediumsTestParam> {};

TEST_P(NearbyConnectionsManagerImplTestMediums, StartAdvertising_Options) {
  const MediumsTestParam& param = GetParam();
  PowerLevel power_level = std::get<0>(param);
  DataUsage data_usage = std::get<1>(param);
  net::NetworkChangeNotifier::ConnectionType connection_type =
      std::get<2>(param);

  network_notifier_->SetConnectionType(connection_type);
  bool should_use_web_rtc =
      data_usage != DataUsage::kOffline &&
      power_level != PowerLevel::kLowPower &&
      connection_type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      (data_usage != DataUsage::kWifiOnly ||
       !net::NetworkChangeNotifier::IsConnectionCellular(connection_type));

  bool is_high_power = power_level == PowerLevel::kHighPower;

  // TODO(crbug.com/1129069): Update when WiFi LAN is supported.
  auto expected_mediums = MediumSelection::New(
      /*bluetooth=*/is_high_power,
      /*ble=*/!is_high_power,
      /*web_rtc=*/should_use_web_rtc,
      /*wifi_lan=*/false);

  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  base::MockCallback<NearbyConnectionsManager::ConnectionsCallback> callback;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;

  EXPECT_CALL(nearby_connections_, StartAdvertising)
      .WillOnce([&](const std::vector<uint8_t>& endpoint_info,
                    const std::string& service_id,
                    AdvertisingOptionsPtr options,
                    mojo::PendingRemote<ConnectionLifecycleListener> listener,
                    NearbyConnectionsMojom::StartAdvertisingCallback callback) {
        EXPECT_EQ(is_high_power, options->auto_upgrade_bandwidth);
        EXPECT_EQ(expected_mediums, options->allowed_mediums);
        EXPECT_EQ(!is_high_power, options->enable_bluetooth_listening);
        std::move(callback).Run(Status::kSuccess);
      });
  EXPECT_CALL(callback, Run(testing::Eq(Status::kSuccess)));

  nearby_connections_manager_.StartAdvertising(
      local_endpoint_info, &incoming_connection_listener, power_level,
      data_usage, callback.Get());
}

INSTANTIATE_TEST_SUITE_P(
    NearbyConnectionsManagerImplTestMediums,
    NearbyConnectionsManagerImplTestMediums,
    testing::Combine(
        testing::Values(PowerLevel::kLowPower, PowerLevel::kHighPower),
        testing::Values(DataUsage::kWifiOnly,
                        DataUsage::kOffline,
                        DataUsage::kOnline),
        testing::Values(net::NetworkChangeNotifier::CONNECTION_NONE,
                        net::NetworkChangeNotifier::CONNECTION_WIFI,
                        net::NetworkChangeNotifier::CONNECTION_3G)));

TEST_F(NearbyConnectionsManagerImplTest, StopAdvertising_BeforeStart) {
  EXPECT_CALL(nearby_connections_, StopAdvertising).Times(0);
  nearby_connections_manager_.StopAdvertising();
}

TEST_F(NearbyConnectionsManagerImplTest, StopAdvertising) {
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(listener_remote, incoming_connection_listener);

  EXPECT_CALL(nearby_connections_, StopAdvertising);
  nearby_connections_manager_.StopAdvertising();
}

TEST_F(NearbyConnectionsManagerImplTest, ShutdownAdvertising) {
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(listener_remote, incoming_connection_listener);

  EXPECT_CALL(nearby_connections_, StopAllEndpoints);
  nearby_connections_manager_.Shutdown();
}

TEST_F(NearbyConnectionsManagerImplTest, ShutdownDiscoveryConnectionFails) {
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  EXPECT_CALL(nearby_connections_, StopAllEndpoints);
  nearby_connections_manager_.Shutdown();

  // RequestConnection will fail.
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  base::RunLoop run_loop;
  NearbyConnection* nearby_connection;
  nearby_connections_manager_.Connect(
      local_endpoint_info, kRemoteEndpointId,
      /*bluetooth_mac_address=*/base::nullopt, DataUsage::kOffline,
      base::BindLambdaForTesting([&](NearbyConnection* connection) {
        nearby_connection = connection;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_FALSE(nearby_connection);
}

TEST_F(NearbyConnectionsManagerImplTest,
       ShutdownBeforeStartDiscoveryOrAdvertising) {
  EXPECT_CALL(nearby_connections_, StopAllEndpoints).Times(0);
  nearby_connections_manager_.Shutdown();
}

TEST_F(NearbyConnectionsManagerImplTest,
       UpgradeBandwidthAfterAdvertisingSucceeds) {
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(listener_remote, incoming_connection_listener);

  // Upgrading bandwidth will succeed.
  EXPECT_CALL(nearby_connections_, InitiateBandwidthUpgrade)
      .WillOnce([&](const std::string& endpoint_id,
                    NearbyConnectionsMojom::InitiateBandwidthUpgradeCallback
                        callback) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        std::move(callback).Run(Status::kSuccess);
      });
  nearby_connections_manager_.UpgradeBandwidth(kRemoteEndpointId);
}

TEST_F(NearbyConnectionsManagerImplTest,
       UpgradeBandwidthAfterDiscoverySucceeds) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  EXPECT_TRUE(nearby_connection);

  // Upgrading bandwidth will succeed.
  EXPECT_CALL(nearby_connections_, InitiateBandwidthUpgrade)
      .WillOnce([&](const std::string& endpoint_id,
                    NearbyConnectionsMojom::InitiateBandwidthUpgradeCallback
                        callback) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        std::move(callback).Run(Status::kSuccess);
      });
  nearby_connections_manager_.UpgradeBandwidth(kRemoteEndpointId);
}

TEST_F(NearbyConnectionsManagerImplTest,
       UpgradeBandwidthBeforeStartDiscoveryOrAdvertising) {
  EXPECT_CALL(nearby_connections_, InitiateBandwidthUpgrade).Times(0);
  nearby_connections_manager_.UpgradeBandwidth(kRemoteEndpointId);
}

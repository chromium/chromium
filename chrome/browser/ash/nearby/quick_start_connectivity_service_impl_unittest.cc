// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/quick_start_connectivity_service_impl.h"

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager_impl.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

namespace {

const char kRemoteEndpointId[] = "remote_endpoint_id";
const char kEndpointInfo[] = {0x0d, 0x07, 0x07, 0x07, 0x07};
const char kRemoteEndpointInfo[] = {0x0d, 0x07, 0x06, 0x08, 0x09};
const char kAuthenticationToken[] = "authentication_token";
const char kRawAuthenticationToken[] = {0x00, 0x05, 0x04, 0x03, 0x02};

}  // namespace

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

class QuickStartConnectivityServiceImplTest : public testing::Test {
 public:
  QuickStartConnectivityServiceImplTest() = default;
  QuickStartConnectivityServiceImplTest(
      QuickStartConnectivityServiceImplTest&) = delete;
  QuickStartConnectivityServiceImplTest& operator=(
      QuickStartConnectivityServiceImplTest&) = delete;
  ~QuickStartConnectivityServiceImplTest() override = default;

  // testing::Test
  void SetUp() override {
    quick_start_connectivity_service_ =
        std::make_unique<QuickStartConnectivityServiceImpl>(
            &nearby_process_manager_);

    EXPECT_CALL(nearby_process_manager_, GetNearbyProcessReference)
        .WillRepeatedly([&](ash::nearby::NearbyProcessManager::
                                NearbyProcessStoppedCallback) {
          auto mock_reference_ptr =
              std::make_unique<ash::nearby::MockNearbyProcessManager::
                                   MockNearbyProcessReference>();

          EXPECT_CALL(*(mock_reference_ptr.get()), GetNearbyConnections)
              .WillRepeatedly(
                  testing::ReturnRef(nearby_connections_.shared_remote()));

          return mock_reference_ptr;
        });
  }

  void Cleanup() { quick_start_connectivity_service_->Cleanup(); }

  void ValidateNearbyConnectionsManager(bool should_be_null) {
    if (should_be_null) {
      EXPECT_EQ(nullptr,
                quick_start_connectivity_service_->nearby_connections_manager_);
    } else {
      EXPECT_NE(nullptr,
                quick_start_connectivity_service_->nearby_connections_manager_);
    }
  }

  void StartDiscovery() {
    mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
    testing::NiceMock<MockDiscoveryListener> discovery_listener;
    EXPECT_CALL(nearby_connections_, StartDiscovery)
        .WillOnce([&discovery_listener_remote](
                      const std::string& service_id,
                      DiscoveryOptionsPtr options,
                      mojo::PendingRemote<EndpointDiscoveryListener> listener,
                      NearbyConnectionsMojom::StartDiscoveryCallback callback) {
          discovery_listener_remote.Bind(std::move(listener));
          std::move(callback).Run(
              ::nearby::connections::mojom::Status::kSuccess);
        });

    base::RunLoop run_loop;
    NearbyConnectionsManager::ConnectionsCallback callback =
        base::BindLambdaForTesting(
            [&run_loop](::nearby::connections::mojom::Status status) {
              EXPECT_EQ(status, ::nearby::connections::mojom::Status::kSuccess);
              run_loop.Quit();
            });

    quick_start_connectivity_service_->GetNearbyConnectionsManager()
        ->StartDiscovery(&discovery_listener,
                         ::nearby_share::mojom::DataUsage::kWifiOnly,
                         std::move(callback));

    run_loop.Run();
  }

  NearbyConnection* Connect() {
    StartDiscovery();
    mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
    mojo::Remote<PayloadListener> payload_listener_remote;

    const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                   std::end(kEndpointInfo));
    const std::vector<uint8_t> remote_endpoint_info(
        std::begin(kRemoteEndpointInfo), std::end(kRemoteEndpointInfo));
    const std::vector<uint8_t> raw_authentication_token(
        std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

    base::RunLoop request_connection_run_loop;
    EXPECT_CALL(nearby_connections_, RequestConnection)
        .WillOnce(
            [&](const std::string& service_id,
                const std::vector<uint8_t>& endpoint_info,
                const std::string& endpoint_id, ConnectionOptionsPtr options,
                mojo::PendingRemote<ConnectionLifecycleListener> listener,
                NearbyConnectionsMojom::RequestConnectionCallback callback) {
              connection_listener_remote.Bind(std::move(listener));
              std::move(callback).Run(
                  ::nearby::connections::mojom::Status::kSuccess);
              request_connection_run_loop.Quit();
            });

    NearbyConnection* nearby_connection;

    quick_start_connectivity_service_->GetNearbyConnectionsManager()->Connect(
        local_endpoint_info, kRemoteEndpointId,
        /*bluetooth_mac_address=*/absl::nullopt, DataUsage::kOffline,
        base::BindLambdaForTesting([&](NearbyConnection* connection) {
          nearby_connection = connection;
        }));

    request_connection_run_loop.Run();
    base::RunLoop run_loop;

    EXPECT_CALL(nearby_connections_, AcceptConnection)
        .WillOnce(
            [&](const std::string& service_id, const std::string& endpoint_id,
                mojo::PendingRemote<PayloadListener> listener,
                NearbyConnectionsMojom::AcceptConnectionCallback callback) {
              payload_listener_remote.Bind(std::move(listener));
              std::move(callback).Run(
                  ::nearby::connections::mojom::Status::kSuccess);
              run_loop.Quit();
            });

    connection_listener_remote->OnConnectionInitiated(
        kRemoteEndpointId, ::nearby::connections::mojom::ConnectionInfo::New(
                               kAuthenticationToken, raw_authentication_token,
                               remote_endpoint_info,
                               /*is_incoming_connection=*/false));
    connection_listener_remote->OnConnectionAccepted(kRemoteEndpointId);
    run_loop.Run();

    return nearby_connection;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<QuickStartConnectivityServiceImpl>
      quick_start_connectivity_service_;
  testing::NiceMock<ash::nearby::MockNearbyConnections> nearby_connections_;
  testing::NiceMock<ash::nearby::MockNearbyProcessManager>
      nearby_process_manager_;
  base::WeakPtrFactory<QuickStartConnectivityServiceImplTest> weak_ptr_factory_{
      this};
};

// Regression test for b/303675257.
TEST_F(QuickStartConnectivityServiceImplTest,
       CleanupNearbyConnectionsMangagerDuringDisconnect) {
  NearbyConnection* nearby_connection = Connect();
  ASSERT_TRUE(nearby_connection);
  ValidateNearbyConnectionsManager(/*should_be_null=*/false);

  // Check that the QuickStartConnectivityServiceImpl::Cleanup() method
  // successfully finishes and resets the |nearby_connections_manager_| during
  // the nearby_connection disconnect process.
  nearby_connection->SetDisconnectionListener(
      base::BindOnce(&QuickStartConnectivityServiceImplTest::Cleanup,
                     weak_ptr_factory_.GetWeakPtr()));
  nearby_connection->Close();
  ValidateNearbyConnectionsManager(/*should_be_null=*/true);
}

}  // namespace ash::quick_start

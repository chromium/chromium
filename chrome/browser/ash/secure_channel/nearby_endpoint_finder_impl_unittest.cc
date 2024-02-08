// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/secure_channel/nearby_endpoint_finder_impl.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace secure_channel {
namespace {

using ::nearby::connections::mojom::DiscoveredEndpointInfo;
using ::nearby::connections::mojom::DiscoveredEndpointInfoPtr;
using ::nearby::connections::mojom::EndpointDiscoveryListener;
using ::nearby::connections::mojom::Status;
using ::testing::_;
using ::testing::Invoke;

const std::vector<uint8_t> GetEid() {
  return std::vector<uint8_t>{0, 1};
}

const std::vector<uint8_t>& GetBluetoothAddress() {
  static const std::vector<uint8_t> address{0, 1, 2, 3, 4, 5};
  return address;
}

}  // namespace

class NearbyEndpointFinderImplTest : public testing::Test {
 protected:
  NearbyEndpointFinderImplTest() = default;
  ~NearbyEndpointFinderImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    finder_ = NearbyEndpointFinderImpl::Factory::Create(
        mock_nearby_connections_.shared_remote());
  }

  void FindEndpoint() {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_nearby_connections_, StartDiscovery(_, _, _, _))
        .WillOnce(Invoke(
            [&](const std::string& service_id, DiscoveryOptionsPtr options,
                mojo::PendingRemote<EndpointDiscoveryListener> listener,
                NearbyConnectionsMojom::StartDiscoveryCallback callback) {
              start_discovery_callback_ = std::move(callback);
              endpoint_discovery_listener_.Bind(std::move(listener));
              run_loop.Quit();
            }));

    finder_->FindEndpoint(
        GetBluetoothAddress(), GetEid(),
        base::BindOnce(&NearbyEndpointFinderImplTest::OnEndpointFound,
                       base::Unretained(this)),
        base::BindOnce(
            &NearbyEndpointFinderImplTest::OnEndpointDiscoveryFailure,
            base::Unretained(this)));

    run_loop.Run();
  }

  void InvokeStartDiscoveryCallback(bool success) {
    base::RunLoop run_loop;

    if (!success) {
      result_closure_ = run_loop.QuitClosure();
      std::move(start_discovery_callback_).Run(Status::kError);
      run_loop.Run();
      return;
    }

    EXPECT_CALL(mock_nearby_connections_,
                InjectBluetoothEndpoint(_, _, _, _, _))
        .WillOnce(Invoke(
            [&](const std::string& service_id, const std::string& endpoint_id,
                const std::vector<uint8_t>& endpoint_info,
                const std::vector<uint8_t>& remote_bluetooth_mac_address,
                NearbyConnectionsMojom::InjectBluetoothEndpointCallback
                    callback) {
              endpoint_id_ = endpoint_id;
              endpoint_info_ = endpoint_info;
              inject_endpoint_callback_ = std::move(callback);
              run_loop.Quit();
            }));

    std::move(start_discovery_callback_).Run(Status::kSuccess);
    run_loop.Run();
  }

  void InvokeInjectEndpointCallback(bool success) {
    base::RunLoop run_loop;

    if (!success) {
      result_closure_ = run_loop.QuitClosure();
      std::move(inject_endpoint_callback_).Run(Status::kError);
      run_loop.Run();
      return;
    }

    std::move(inject_endpoint_callback_).Run(Status::kSuccess);
  }

  void InvokeOnEndpointFound() {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_nearby_connections_, StopDiscovery(_, _))
        .WillOnce(
            Invoke([&](const std::string& service_id,
                       NearbyConnectionsMojom::StopDiscoveryCallback callback) {
              stop_discovery_callback_ = std::move(callback);
              run_loop.Quit();
            }));

    endpoint_discovery_listener_->OnEndpointFound(
        endpoint_id_,
        DiscoveredEndpointInfo::New(endpoint_info_, mojom::kServiceId));
    run_loop.Run();
  }

  void InvokeStopDiscoveryCallback(bool success) {
    base::RunLoop run_loop;
    result_closure_ = run_loop.QuitClosure();
    std::move(stop_discovery_callback_)
        .Run(success ? Status::kSuccess : Status::kError);
    run_loop.Run();
  }

  void DeleteFinder(bool expected_to_stop_discovery) {
    if (!expected_to_stop_discovery) {
      EXPECT_CALL(mock_nearby_connections_, StopDiscovery(_, _)).Times(0);
      finder_.reset();
      return;
    }

    base::RunLoop run_loop;
    EXPECT_CALL(mock_nearby_connections_, StopDiscovery(_, _))
        .WillOnce(
            Invoke([&](const std::string& service_id,
                       NearbyConnectionsMojom::StopDiscoveryCallback callback) {
              std::move(callback).Run(Status::kSuccess);
              run_loop.Quit();
            }));

    finder_.reset();
    run_loop.Run();
  }

  bool has_failed_ = false;

 private:
  void OnEndpointFound(const std::string& endpoint_id,
                       DiscoveredEndpointInfoPtr endpoint_info) {
    EXPECT_EQ(endpoint_id_, endpoint_id);
    std::move(result_closure_).Run();
  }

  void OnEndpointDiscoveryFailure(::nearby::connections::mojom::Status status) {
    has_failed_ = true;
    std::move(result_closure_).Run();
  }

  base::test::TaskEnvironment task_environment_;
  nearby::MockNearbyConnections mock_nearby_connections_;

  std::unique_ptr<NearbyEndpointFinder> finder_;

  base::OnceClosure result_closure_;
  NearbyConnectionsMojom::StartDiscoveryCallback start_discovery_callback_;
  std::string endpoint_id_;
  std::vector<uint8_t> endpoint_info_;
  NearbyConnectionsMojom::InjectBluetoothEndpointCallback
      inject_endpoint_callback_;
  NearbyConnectionsMojom::StopDiscoveryCallback stop_discovery_callback_;

  mojo::Remote<EndpointDiscoveryListener> endpoint_discovery_listener_;
};

TEST_F(NearbyEndpointFinderImplTest, Success) {
  FindEndpoint();
  InvokeStartDiscoveryCallback(/*success=*/true);
  InvokeInjectEndpointCallback(/*success=*/true);
  InvokeOnEndpointFound();
  InvokeStopDiscoveryCallback(/*success=*/true);
  DeleteFinder(/*expected_to_stop_discovery=*/false);

  EXPECT_FALSE(has_failed_);
}

TEST_F(NearbyEndpointFinderImplTest, FailStartingDiscovery) {
  FindEndpoint();
  InvokeStartDiscoveryCallback(/*success=*/false);
  DeleteFinder(/*expected_to_stop_discovery=*/false);

  EXPECT_TRUE(has_failed_);
}

// Failing on CrOS ASAN: crbug.com/1290882
TEST_F(NearbyEndpointFinderImplTest, DISABLED_FailInjectingEndpoint) {
  FindEndpoint();
  InvokeStartDiscoveryCallback(/*success=*/true);
  InvokeInjectEndpointCallback(/*success=*/false);
  DeleteFinder(/*expected_to_stop_discovery=*/true);

  EXPECT_TRUE(has_failed_);
}

TEST_F(NearbyEndpointFinderImplTest, FailStoppingDiscovery) {
  FindEndpoint();
  InvokeStartDiscoveryCallback(/*success=*/true);
  InvokeInjectEndpointCallback(/*success=*/true);
  InvokeOnEndpointFound();
  InvokeStopDiscoveryCallback(/*success=*/false);
  DeleteFinder(/*expected_to_stop_discovery=*/false);

  EXPECT_TRUE(has_failed_);
}

}  // namespace secure_channel
}  // namespace ash

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ash/crosapi/test/crosapi_test_base.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/network_change.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::AnyNumber;

namespace crosapi {
namespace {

constexpr char kWifiServicePath[] = "/service/wifi1";
constexpr char kEthServicePath[] = "/service/eth1";

class MockNetworkChangeObserver : public mojom::NetworkChangeObserver {
 public:
  // This mock is for catching the input given via crosapi, but not for
  // modifying the crosapi behavior.
  MOCK_METHOD(void,
              OnNetworkChanged,
              (bool dns_changed,
               bool ip_address_changed,
               bool connection_type_changed,
               mojom::ConnectionType new_connection_type,
               bool connection_subtype_changed,
               mojom::ConnectionSubtype new_connection_subtype),
              (override));
};

class NetworkChangeCrosapiTest : public CrosapiTestBase {
 protected:
  void SetUp() override {
    CrosapiTestBase::SetUp();

    network_change_ = BindCrosapiInterface(&mojom::Crosapi::BindNetworkChange);
    test_controller_ =
        BindCrosapiInterface(&mojom::Crosapi::BindTestController);
  }

  mojo::Remote<mojom::NetworkChange> network_change_;
  mojo::Remote<mojom::TestController> test_controller_;

  MockNetworkChangeObserver observer_;
  mojo::Receiver<mojom::NetworkChangeObserver> receiver_{&observer_};
};

TEST_F(NetworkChangeCrosapiTest, OnNetworkChanged) {
  base::RunLoop run_loop;
  // When NetworkChange::AddObserver() is called,
  // NetworkChangeObserver::OnNetworkChange() should also be called to
  // initialize network setup.
  EXPECT_CALL(observer_,
              OnNetworkChanged(
                  /*dns_changed=*/false, /*ip_address_changed=*/false,
                  /*connection_type_changed=*/true,
                  mojom::ConnectionType(
                      net::NetworkChangeNotifier::CONNECTION_ETHERNET),
                  /*connection_subtype_changed=*/true,
                  mojom::ConnectionSubtype(
                      net::NetworkChangeNotifier::SUBTYPE_UNKNOWN)))
      .WillOnce([&] { run_loop.Quit(); });
  network_change_->AddObserver(receiver_.BindNewPipeAndPassRemote());
  run_loop.Run();

  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer_));

  {
    testing::InSequence sequence;

    base::RunLoop run_loop2;
    // Ignore OnNetworkChange() calls and only check the last result which
    // overrides all results passed before. The first called
    // DisconnectFromNetwork() causes a few network chaneges for some reason.
    // TODO(crbug.com/1374276): Modify network change crosapi specification.
    EXPECT_CALL(observer_, OnNetworkChanged(_, _, _, _, _, _))
        .Times(AnyNumber());
    // Check if eventually disconnected from Ethernet and connected to Wifi.
    EXPECT_CALL(observer_, OnNetworkChanged(
                               /*dns_changed=*/true,
                               /*ip_address_changed=*/true,
                               /*connection_type_changed=*/true,
                               mojom::ConnectionType::CONNECTION_WIFI,
                               /*connection_subtype_changed=*/true,
                               mojom::ConnectionSubtype::SUBTYPE_UNKNOWN))
        .WillOnce([&] { run_loop2.Quit(); });

    test_controller_->DisconnectFromNetwork(kEthServicePath);
    run_loop2.Run();
  }
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer_));

  base::RunLoop run_loop3;
  // Check if disconnected from Wifi and not connected to anything.
  EXPECT_CALL(observer_, OnNetworkChanged(
                             /*dns_changed=*/true,
                             /*ip_address_changed=*/true,
                             /*connection_type_changed=*/true,
                             mojom::ConnectionType::CONNECTION_NONE,
                             /*connection_subtype_changed=*/true,
                             mojom::ConnectionSubtype::SUBTYPE_NONE))
      .WillOnce([&] { run_loop3.Quit(); });

  test_controller_->DisconnectFromNetwork(kWifiServicePath);
  run_loop3.Run();

  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer_));

  base::RunLoop run_loop4;
  // Check if connected to Ethernet.
  EXPECT_CALL(observer_, OnNetworkChanged(
                             /*dns_changed=*/true,
                             /*ip_address_changed=*/true,
                             /*connection_type_changed=*/true,
                             mojom::ConnectionType::CONNECTION_ETHERNET,
                             /*connection_subtype_changed=*/true,
                             mojom::ConnectionSubtype::SUBTYPE_UNKNOWN))
      .WillOnce([&] { run_loop4.Quit(); });

  test_controller_->ConnectToNetwork(kEthServicePath);
  run_loop4.Run();
}

}  // namespace
}  // namespace crosapi

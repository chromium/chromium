// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

class NetObserver : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  NetObserver() {
    net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
    last_connection_type_ = net::NetworkChangeNotifier::GetConnectionType();
  }

  ~NetObserver() override {
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  void WaitForConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    while (last_connection_type_ != type) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  // net::NetworkChangeNotifier:NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    change_count_++;
    last_connection_type_ = type;

    // TODO(b/229673213): Remove log once flakiness is fixed.
    LOG(INFO) << "NetworkChangeObserver was called, change count increased to "
              << change_count_
              << " Last connection type is now: " << last_connection_type_;
    if (run_loop_)
      run_loop_->Quit();
  }

  int change_count_ = 0;
  net::NetworkChangeNotifier::ConnectionType last_connection_type_;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

class NetworkServiceObserver
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  NetworkServiceObserver() {
    content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
    // TODO(b/229673213): Remove log once flakiness is fixed.
    LOG(INFO) << "NetworkServiceObserver get connection type";
    content::GetNetworkConnectionTracker()->GetConnectionType(
        &last_connection_type_,
        base::BindOnce(&NetworkServiceObserver::OnConnectionChanged,
                       weak_factory_.GetWeakPtr()));
  }

  ~NetworkServiceObserver() override {
    content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
        this);
  }

  void WaitForConnectionType(network::mojom::ConnectionType type) {
    while (last_connection_type_ != type) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    change_count_++;
    last_connection_type_ = type;

    // TODO(b/229673213): Remove log once flakiness is fixed.
    LOG(INFO) << "NetworkServiceObserver was called, change count increased to "
              << change_count_
              << " Last connection type is now: " << last_connection_type_;
    if (run_loop_)
      run_loop_->Quit();
  }

  int change_count_ = 0;
  network::mojom::ConnectionType last_connection_type_;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  base::WeakPtrFactory<NetworkServiceObserver> weak_factory_{this};
};

}  // namespace

class NetworkChangeManagerClientBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Make sure everyone thinks we have an ethernet connection.
    NetObserver().WaitForConnectionType(
        net::NetworkChangeNotifier::CONNECTION_ETHERNET);
    NetworkServiceObserver().WaitForConnectionType(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);

    // Wait for all services to be removed.
    ShillServiceClient::Get()->GetTestInterface()->ClearServices();
    base::RunLoop().RunUntilIdle();
  }

  ShillServiceClient::TestInterface* service_client() {
    return ShillServiceClient::Get()->GetTestInterface();
  }
};

// Tests that network changes from shill are received by both the
// NetworkChangeNotifier and NetworkConnectionTracker.
IN_PROC_BROWSER_TEST_F(NetworkChangeManagerClientBrowserTest,
                       ReceiveNotifications) {
  NetObserver net_observer;
  NetworkServiceObserver network_service_observer;

  service_client()->AddService("wifi", "wifi", "wifi", shill::kTypeWifi,
                               shill::kStateOnline, true);

  net_observer.WaitForConnectionType(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  // NetworkChangeNotifier will send a CONNECTION_NONE notification before
  // the CONNECTION_WIFI one.
  EXPECT_EQ(2, net_observer.change_count_);
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_WIFI,
            net_observer.last_connection_type_);

  network_service_observer.WaitForConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(2, network_service_observer.change_count_);
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_WIFI,
            network_service_observer.last_connection_type_);
}

// Tests that the NetworkChangeManagerClient reconnects to the network service
// after it gets disconnected.
IN_PROC_BROWSER_TEST_F(NetworkChangeManagerClientBrowserTest,
                       ReconnectToNetworkService) {
  NetworkServiceObserver network_service_observer;

  // Manually call SimulateCrash instead of
  // BrowserTestBase::SimulateNetworkServiceCrash to avoid the cleanup and
  // reconnection work it does for you.
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());
  IgnoreNetworkServiceCrashes();
  network_service_test->SimulateCrash();

  service_client()->AddService("wifi", "wifi", "wifi", shill::kTypeWifi,
                               shill::kStateOnline, true);

  NetObserver().WaitForConnectionType(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  network_service_observer.WaitForConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(2, network_service_observer.change_count_);
}

}  // namespace ash

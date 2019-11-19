// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

namespace {

class TestNetworkConnectionObserver
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit TestNetworkConnectionObserver(
      network::NetworkConnectionTracker* tracker)
      : num_notifications_(0),
        tracker_(tracker),
        run_loop_(std::make_unique<base::RunLoop>()),
        connection_type_(network::mojom::ConnectionType::CONNECTION_UNKNOWN) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    tracker_->AddNetworkConnectionObserver(this);
  }

  ~TestNetworkConnectionObserver() override {
    tracker_->RemoveNetworkConnectionObserver(this);
  }

  // NetworkConnectionObserver implementation:
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    network::mojom::ConnectionType queried_type;
    bool sync = tracker_->GetConnectionType(
        &queried_type,
        base::BindOnce([](network::mojom::ConnectionType type) {}));
    EXPECT_TRUE(sync);
    EXPECT_EQ(type, queried_type);

    num_notifications_++;
    connection_type_ = type;
    run_loop_->Quit();
  }

  void WaitForNotification() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  size_t num_notifications() const { return num_notifications_; }
  network::mojom::ConnectionType connection_type() const {
    return connection_type_;
  }

 private:
  size_t num_notifications_;
  network::NetworkConnectionTracker* tracker_;
  std::unique_ptr<base::RunLoop> run_loop_;
  network::mojom::ConnectionType connection_type_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(TestNetworkConnectionObserver);
};

}  // namespace

class NetworkConnectionTrackerBrowserTest : public InProcessBrowserTest {
 public:
  NetworkConnectionTrackerBrowserTest() {}
  ~NetworkConnectionTrackerBrowserTest() override {}

  // Simulates a network connection change.
  void SimulateNetworkChange(network::mojom::ConnectionType type) {
    if (!content::IsInProcessNetworkService()) {
      mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
      content::GetNetworkService()->BindTestInterface(
          network_service_test.BindNewPipeAndPassReceiver());
      base::RunLoop run_loop;
      network_service_test->SimulateNetworkChange(
          type, base::Bind([](base::RunLoop* run_loop) { run_loop->Quit(); },
                           base::Unretained(&run_loop)));
      run_loop.Run();
      return;
    }
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        net::NetworkChangeNotifier::ConnectionType(type));
  }

 private:
};

// Basic test to make sure NetworkConnectionTracker is set up.
IN_PROC_BROWSER_TEST_F(NetworkConnectionTrackerBrowserTest,
                       NetworkConnectionTracker) {
#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
  // NetworkService on ChromeOS doesn't yet have a NetworkChangeManager
  // implementation. OSX uses a separate binary for service processes and
  // browser test fixture doesn't have NetworkServiceTest mojo code.
  return;
#endif
  network::NetworkConnectionTracker* tracker =
      content::GetNetworkConnectionTracker();
  EXPECT_NE(nullptr, tracker);
  // Issue a GetConnectionType() request to make sure NetworkService has been
  // started up. This way, NetworkService will receive the broadcast when
  // SimulateNetworkChange() is called.
  base::RunLoop run_loop;
  network::mojom::ConnectionType ignored_type;
  bool sync = tracker->GetConnectionType(
      &ignored_type,
      base::BindOnce(
          [](base::RunLoop* run_loop, network::mojom::ConnectionType type) {
            run_loop->Quit();
          },
          base::Unretained(&run_loop)));
  if (!sync)
    run_loop.Run();
  TestNetworkConnectionObserver network_connection_observer(tracker);
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  network_connection_observer.WaitForNotification();
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_3G,
            network_connection_observer.connection_type());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, network_connection_observer.num_notifications());
}

// Simulates a network service crash, and ensures that network change manager
// binds to the restarted network service.
IN_PROC_BROWSER_TEST_F(NetworkConnectionTrackerBrowserTest,
                       SimulateNetworkServiceCrash) {
  // Out-of-process network service is not enabled, so network service's crash
  // and restart aren't applicable.
  if (!content::IsOutOfProcessNetworkService())
    return;

  network::NetworkConnectionTracker* tracker =
      content::GetNetworkConnectionTracker();
  EXPECT_NE(nullptr, tracker);

  // Issue a GetConnectionType() request to make sure NetworkService has been
  // started up. This way, NetworkService will receive the broadcast when
  // SimulateNetworkChange() is called.
  {
    base::RunLoop run_loop;
    network::mojom::ConnectionType ignored_type;
    bool sync = tracker->GetConnectionType(
        &ignored_type,
        base::BindOnce(
            [](base::RunLoop* run_loop, network::mojom::ConnectionType type) {
              run_loop->Quit();
            },
            base::Unretained(&run_loop)));
    if (!sync)
      run_loop.Run();
  }

  TestNetworkConnectionObserver network_connection_observer(tracker);
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);

  network_connection_observer.WaitForNotification();
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_3G,
            network_connection_observer.connection_type());
  // Wait a bit longer to make sure only 1 notification is received and that
  // there is no duplicate notification.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, network_connection_observer.num_notifications());

  SimulateNetworkServiceCrash();

  // Issue a GetConnectionType() request to make sure NetworkService has been
  // started up. This way, NetworkService will receive the broadcast when
  // SimulateNetworkChange() is called.
  {
    base::RunLoop run_loop;
    network::mojom::ConnectionType ignored_type;
    bool sync = tracker->GetConnectionType(
        &ignored_type,
        base::BindOnce(
            [](base::RunLoop* run_loop, network::mojom::ConnectionType type) {
              run_loop->Quit();
            },
            base::Unretained(&run_loop)));
    if (!sync)
      run_loop.Run();
  }

  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_2G);
  network_connection_observer.WaitForNotification();
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_2G,
            network_connection_observer.connection_type());

  // Wait a bit longer to make sure only 2 notifications are received.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, network_connection_observer.num_notifications());
}

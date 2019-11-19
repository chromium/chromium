// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

namespace network {

namespace {

// Simulates a network quality change. This is only called when network service
// is running in the browser process, in which case, the network quality
// estimator lives on the network thread.
void SimulateNetworkQualityChangeOnNetworkThread(
    net::EffectiveConnectionType type) {
  network::NetworkService::GetNetworkServiceForTesting()
      ->network_quality_estimator()
      ->SimulateNetworkQualityChangeForTesting(type);
  base::RunLoop().RunUntilIdle();
}

class TestNetworkQualityObserver
    : public NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  explicit TestNetworkQualityObserver(NetworkQualityTracker* tracker)
      : num_notifications_(0),
        run_loop_wait_effective_connection_type_(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
        run_loop_(std::make_unique<base::RunLoop>()),
        tracker_(tracker),
        effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    tracker_->AddEffectiveConnectionTypeObserver(this);
  }

  ~TestNetworkQualityObserver() override {
    tracker_->RemoveEffectiveConnectionTypeObserver(this);
  }

  // NetworkQualityTracker::EffectiveConnectionTypeObserver implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    net::EffectiveConnectionType queried_type =
        tracker_->GetEffectiveConnectionType();
    EXPECT_EQ(type, queried_type);

    num_notifications_++;
    effective_connection_type_ = type;
    if (effective_connection_type_ != run_loop_wait_effective_connection_type_)
      return;
    run_loop_->Quit();
  }

  void WaitForNotification(
      net::EffectiveConnectionType run_loop_wait_effective_connection_type) {
    if (effective_connection_type_ == run_loop_wait_effective_connection_type)
      return;
    ASSERT_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
              run_loop_wait_effective_connection_type);
    run_loop_wait_effective_connection_type_ =
        run_loop_wait_effective_connection_type;
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop());
  }

  size_t num_notifications() const { return num_notifications_; }
  net::EffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }
  base::TimeDelta http_rtt() const { return tracker_->GetHttpRTT(); }
  base::TimeDelta transport_rtt() const { return tracker_->GetTransportRTT(); }
  int32_t downlink_bandwidth_kbps() const {
    return tracker_->GetDownstreamThroughputKbps();
  }

 private:
  size_t num_notifications_;
  net::EffectiveConnectionType run_loop_wait_effective_connection_type_;
  std::unique_ptr<base::RunLoop> run_loop_;
  NetworkQualityTracker* tracker_;
  net::EffectiveConnectionType effective_connection_type_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkQualityObserver);
};

}  // namespace

class NetworkQualityTrackerBrowserTest : public InProcessBrowserTest {
 public:
  NetworkQualityTrackerBrowserTest() {}
  ~NetworkQualityTrackerBrowserTest() override {}

  // Simulates a network quality change.
  void SimulateNetworkQualityChange(net::EffectiveConnectionType type) {
    if (!content::IsOutOfProcessNetworkService()) {
      scoped_refptr<base::SequencedTaskRunner> task_runner =
          base::CreateSequencedTaskRunner({content::BrowserThread::IO});
      if (content::IsInProcessNetworkService())
        task_runner = content::GetNetworkTaskRunner();
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&SimulateNetworkQualityChangeOnNetworkThread, type));
      return;
    }

    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    content::StoragePartition* partition =
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile());
    DCHECK(partition->GetNetworkContext());
    DCHECK(content::GetNetworkService());

    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterface(
        network_service_test.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    network_service_test->SimulateNetworkQualityChange(
        type, base::BindOnce([](base::RunLoop* run_loop) { run_loop->Quit(); },
                             base::Unretained(&run_loop)));
    run_loop.Run();
  }
};

// Basic test to make sure NetworkQualityTracker is set up, and observers are
// notified.
IN_PROC_BROWSER_TEST_F(NetworkQualityTrackerBrowserTest,
                       NetworkQualityTracker) {
  // Change the network quality to UNKNOWN to prevent any spurious
  // notifications.
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  base::RunLoop().RunUntilIdle();

  NetworkQualityTracker* tracker = g_browser_process->network_quality_tracker();
  EXPECT_NE(nullptr, tracker);

  base::RunLoop run_loop;
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_4G);
  TestNetworkQualityObserver network_quality_observer(tracker);
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_4G,
            network_quality_observer.effective_connection_type());

  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_3G);
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
            network_quality_observer.effective_connection_type());

  base::RunLoop().RunUntilIdle();

  // Verify that not too many effective connection type observations are
  // received. Note that setting the effective connection type to UNKNOWN above,
  // and adding the observer is racy. In some cases, the observer may receiver
  // the notification about effective connection type being UNKNOWN, followed
  // by other notifications.
  EXPECT_LE(1u, network_quality_observer.num_notifications());
  EXPECT_GE(5u, network_quality_observer.num_notifications());

  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(450),
            network_quality_observer.http_rtt());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(400),
            network_quality_observer.transport_rtt());
  EXPECT_EQ(400, network_quality_observer.downlink_bandwidth_kbps());
}

// Basic test to make sure NetworkQualityTracker is set up, and clients are
// notified as soon as they request notifications from the
// NetworkQualityEstimatorManager.
IN_PROC_BROWSER_TEST_F(NetworkQualityTrackerBrowserTest,
                       NetworkQualityTrackerNotifiedOnInitialization) {
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_2G);
  base::RunLoop().RunUntilIdle();

  NetworkQualityTracker* tracker = g_browser_process->network_quality_tracker();
  EXPECT_NE(nullptr, tracker);

  base::RunLoop run_loop;
  TestNetworkQualityObserver network_quality_observer(tracker);
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_quality_observer.effective_connection_type());
  // Typical RTT and downlink values when effective connection type is 2G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1800),
            network_quality_observer.http_rtt());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500),
            network_quality_observer.transport_rtt());
  EXPECT_EQ(75, network_quality_observer.downlink_bandwidth_kbps());
}

// Simulates a network service crash, and ensures that network quality estimate
// manager binds to the restarted network service.
IN_PROC_BROWSER_TEST_F(NetworkQualityTrackerBrowserTest,
                       SimulateNetworkServiceCrash) {
  // Network service is not running out of process, so cannot be crashed.
  if (!content::IsOutOfProcessNetworkService())
    return;

  // Change the network quality to UNKNOWN to prevent any spurious
  // notifications.
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  base::RunLoop().RunUntilIdle();

  NetworkQualityTracker* tracker = g_browser_process->network_quality_tracker();
  EXPECT_NE(nullptr, tracker);

  base::RunLoop run_loop;
  TestNetworkQualityObserver network_quality_observer(tracker);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_quality_observer.effective_connection_type());

  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_3G);
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
            network_quality_observer.effective_connection_type());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, network_quality_observer.num_notifications());
  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(450),
            network_quality_observer.http_rtt());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(400),
            network_quality_observer.transport_rtt());
  EXPECT_EQ(400, network_quality_observer.downlink_bandwidth_kbps());

  SimulateNetworkServiceCrash();
  // Flush the network interface to make sure it notices the crash.
  content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
      ->FlushNetworkInterfaceForTesting();

  base::RunLoop().RunUntilIdle();

  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_2G);
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_LE(2u, network_quality_observer.num_notifications());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1800),
            network_quality_observer.http_rtt());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500),
            network_quality_observer.transport_rtt());
  EXPECT_EQ(75, network_quality_observer.downlink_bandwidth_kbps());
}

}  // namespace network

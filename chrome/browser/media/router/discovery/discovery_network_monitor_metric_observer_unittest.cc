// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_monitor_metric_observer.h"

#include <memory>

#include "base/test/task_environment.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

std::ostream& operator<<(
    std::ostream& os,
    DiscoveryNetworkMonitorConnectionType connection_type) {
  switch (connection_type) {
    case DiscoveryNetworkMonitorConnectionType::kWifi:
      os << "kWifi";
      break;
    case DiscoveryNetworkMonitorConnectionType::kEthernet:
      os << "kEthernet";
      break;
    case DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsWifi:
      os << "kUnknownReportedAsWifi";
      break;
    case DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsEthernet:
      os << "kUnknownReportedAsEthernet";
      break;
    case DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsOther:
      os << "kUnknownReportedAsOther";
      break;
    case DiscoveryNetworkMonitorConnectionType::kUnknown:
      os << "kUnknown";
      break;
    case DiscoveryNetworkMonitorConnectionType::kDisconnected:
      os << "kDisconnected";
      break;
    default:
      os << "Bad DiscoveryNetworkMonitorConnectionType value";
      break;
  }
  return os;
}

namespace {

using ::testing::_;

class MockMetrics : public DiscoveryNetworkMonitorMetrics {
 public:
  MOCK_METHOD1(RecordTimeBetweenNetworkChangeEvents, void(base::TimeDelta));
  MOCK_METHOD1(RecordConnectionType,
               void(DiscoveryNetworkMonitorConnectionType));
};

class DiscoveryNetworkMonitorMetricObserverTest : public ::testing::Test {
 public:
  DiscoveryNetworkMonitorMetricObserverTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        start_ticks_(task_environment_.NowTicks()),
        metrics_(std::make_unique<MockMetrics>()),
        mock_metrics_(metrics_.get()),
        metric_observer_(task_environment_.GetMockTickClock(),
                         std::move(metrics_)) {
    SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  }

 protected:
  void SetConnectionType(network::mojom::ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::TimeDelta time_advance_ = base::TimeDelta::FromMilliseconds(10);
  const base::TimeTicks start_ticks_;
  std::unique_ptr<MockMetrics> metrics_;
  MockMetrics* mock_metrics_;

  DiscoveryNetworkMonitorMetricObserver metric_observer_;
};

}  // namespace

TEST_F(DiscoveryNetworkMonitorMetricObserverTest, RecordsFirstGoodNetworkWifi) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kWifi));
  metric_observer_.OnNetworksChanged("network1");
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkEthernet) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  metric_observer_.OnNetworksChanged("network1");
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkUnknownWifi) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(
          DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsWifi));
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdUnknown);
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkUnknownEthernet) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(
          DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsEthernet));
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdUnknown);
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkUnknownOther) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_4G);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(
          DiscoveryNetworkMonitorConnectionType::kUnknownReportedAsOther));
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdUnknown);
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkUnknown) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kUnknown));
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdUnknown);
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsFirstGoodNetworkDisconnected) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(*mock_metrics_,
              RecordConnectionType(
                  DiscoveryNetworkMonitorConnectionType::kDisconnected));
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       DoesntRecordEphemeralDisconnectedState) {
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  metric_observer_.OnNetworksChanged("network1");

  EXPECT_CALL(*mock_metrics_, RecordConnectionType(_)).Times(0);
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_));
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  metric_observer_.OnNetworksChanged("network2");

  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       DoesntRecordEphemeralDisconnectedStateWhenFirst) {
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(*mock_metrics_, RecordConnectionType(_)).Times(0);
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  metric_observer_.OnNetworksChanged("network2");

  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordsTimeChangeBetweenConnectionTypeEvents) {
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  metric_observer_.OnNetworksChanged("network1");

  task_environment_.FastForwardBy(time_advance_);
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  task_environment_.FastForwardBy(time_advance_);
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_CALL(*mock_metrics_,
              RecordTimeBetweenNetworkChangeEvents(
                  (start_ticks_ + time_advance_ * 2) - start_ticks_));
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  metric_observer_.OnNetworksChanged("network2");

  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordChangeToDisconnectedState) {
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  metric_observer_.OnNetworksChanged("network1");

  task_environment_.FastForwardBy(time_advance_);
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  task_environment_.FastForwardBy(time_advance_);
  EXPECT_CALL(*mock_metrics_,
              RecordTimeBetweenNetworkChangeEvents(
                  (start_ticks_ + time_advance_) - start_ticks_));
  EXPECT_CALL(*mock_metrics_,
              RecordConnectionType(
                  DiscoveryNetworkMonitorConnectionType::kDisconnected));

  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(DiscoveryNetworkMonitorMetricObserverTest,
       RecordChangeFromDisconnectedState) {
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(_)).Times(0);
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  metric_observer_.OnNetworksChanged("network1");

  task_environment_.FastForwardBy(time_advance_);
  const auto disconnect_ticks = task_environment_.NowTicks();
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  metric_observer_.OnNetworksChanged(
      DiscoveryNetworkMonitor::kNetworkIdDisconnected);

  task_environment_.FastForwardBy(time_advance_);
  EXPECT_CALL(*mock_metrics_,
              RecordTimeBetweenNetworkChangeEvents(
                  (start_ticks_ + time_advance_) - start_ticks_));
  EXPECT_CALL(*mock_metrics_,
              RecordConnectionType(
                  DiscoveryNetworkMonitorConnectionType::kDisconnected));

  task_environment_.FastForwardUntilNoTasksRemain();

  task_environment_.FastForwardBy(time_advance_);
  const auto second_ethernet_ticks = task_environment_.NowTicks();
  EXPECT_CALL(*mock_metrics_, RecordTimeBetweenNetworkChangeEvents(
                                  second_ethernet_ticks - disconnect_ticks));
  EXPECT_CALL(
      *mock_metrics_,
      RecordConnectionType(DiscoveryNetworkMonitorConnectionType::kEthernet));
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  metric_observer_.OnNetworksChanged("network1");
}

}  // namespace media_router

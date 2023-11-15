// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using cast_channel::ChannelError;
using ::testing::_;
using testing::ElementsAre;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArgs;
using MockBoolCallback = base::MockCallback<base::OnceCallback<void(bool)>>;

namespace media_router {

namespace {

const char kPubliclyRoutableIPv4Address[] = "172.32.0.0";

MATCHER_P(RetryParamEq, expected, "") {
  return expected.initial_delay_in_milliseconds ==
             arg.initial_delay_in_milliseconds &&
         expected.max_retry_attempts == arg.max_retry_attempts &&
         expected.multiply_factor == arg.multiply_factor;
}

MATCHER_P(OpenParamEq, expected, "") {
  return expected.connect_timeout_in_seconds ==
             arg.connect_timeout_in_seconds &&
         expected.dynamic_timeout_delta_in_seconds ==
             arg.dynamic_timeout_delta_in_seconds &&
         expected.liveness_timeout_in_seconds ==
             arg.liveness_timeout_in_seconds &&
         expected.ping_interval_in_seconds == arg.ping_interval_in_seconds;
}

class MockObserver : public MediaSinkServiceBase::Observer {
 public:
  MockObserver() {}
  ~MockObserver() override = default;

  MOCK_METHOD1(OnSinkAddedOrUpdated, void(const MediaSinkInternal&));
  MOCK_METHOD1(OnSinkRemoved, void(const MediaSinkInternal&));
};

}  // namespace

class CastMediaSinkServiceImplTest : public ::testing::TestWithParam<bool> {
 public:
  CastMediaSinkServiceImplTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        mock_time_task_runner_(new base::TestMockTimeTaskRunner()),
        mock_cast_socket_service_(
            new cast_channel::MockCastSocketService(mock_time_task_runner_)),
        media_sink_service_impl_(
            mock_sink_discovered_cb_.Get(),
            mock_cast_socket_service_.get(),
            discovery_network_monitor_.get(),
            GetParam() ? &dial_media_sink_service_ : nullptr,
            /* allow_all_ips */ false) {
    mock_cast_socket_service_->SetTaskRunnerForTest(mock_time_task_runner_);
    media_sink_service_impl_.AddObserver(&observer_);
  }
  CastMediaSinkServiceImplTest(CastMediaSinkServiceImplTest&) = delete;
  CastMediaSinkServiceImplTest& operator=(CastMediaSinkServiceImplTest&) =
      delete;

  void SetUp() override {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();
    media_sink_service_impl_.SetTimerForTest(std::move(mock_timer));
  }

  void TearDown() override {
    content::RunAllTasksUntilIdle();
    fake_network_info_ = fake_ethernet_info_;
    media_sink_service_impl_.RemoveObserver(&observer_);
  }

  void OpenChannels(const std::vector<MediaSinkInternal>& cast_sinks,
                    CastMediaSinkServiceImpl::SinkSource sink_source) {
    media_sink_service_impl_.OpenChannels(cast_sinks, sink_source);
  }

  void ChangeConnectionType(network::mojom::ConnectionType connection_type) {
    discovery_network_monitor_->OnConnectionChanged(connection_type);
  }

 protected:
  void ExpectOpenSocket(cast_channel::CastSocket* socket) {
    EXPECT_CALL(*mock_cast_socket_service_,
                OpenSocket_(socket->ip_endpoint(), _))
        .WillOnce(base::test::RunOnceCallback<1>(socket));
  }

  static const std::vector<DiscoveryNetworkInfo> fake_ethernet_info_;
  static const std::vector<DiscoveryNetworkInfo> fake_wifi_info_;
  static const std::vector<DiscoveryNetworkInfo> fake_unknown_info_;

  static std::vector<DiscoveryNetworkInfo> FakeGetNetworkInfo() {
    return fake_network_info_;
  }

  static std::vector<DiscoveryNetworkInfo> fake_network_info_;

  const content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;
  std::unique_ptr<DiscoveryNetworkMonitor> discovery_network_monitor_ =
      DiscoveryNetworkMonitor::CreateInstanceForTest(&FakeGetNetworkInfo);

  base::MockCallback<OnSinksDiscoveredCallback> mock_sink_discovered_cb_;
  TestMediaSinkService dial_media_sink_service_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> mock_timer_;
  CastMediaSinkServiceImpl media_sink_service_impl_;
  testing::NiceMock<MockObserver> observer_;
};

// static
const std::vector<DiscoveryNetworkInfo>
    CastMediaSinkServiceImplTest::fake_ethernet_info_ = {
        DiscoveryNetworkInfo{std::string("enp0s2"), std::string("ethernet1")}};
// static
const std::vector<DiscoveryNetworkInfo>
    CastMediaSinkServiceImplTest::fake_wifi_info_ = {
        DiscoveryNetworkInfo{std::string("wlp3s0"), std::string("wifi1")},
        DiscoveryNetworkInfo{std::string("wlp3s1"), std::string("wifi2")}};
// static
const std::vector<DiscoveryNetworkInfo>
    CastMediaSinkServiceImplTest::fake_unknown_info_ = {
        DiscoveryNetworkInfo{std::string("enp0s2"), std::string()}};

// static
std::vector<DiscoveryNetworkInfo>
    CastMediaSinkServiceImplTest::fake_network_info_ =
        CastMediaSinkServiceImplTest::fake_ethernet_info_;

TEST_P(CastMediaSinkServiceImplTest, TestOnChannelOpenSucceeded) {
  auto cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);

  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink));
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink, &socket, CastMediaSinkServiceImpl::SinkSource::kMdns,
      base::DoNothing());

  // Verify sink content
  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_CALL(mock_sink_discovered_cb_,
              Run(std::vector<MediaSinkInternal>({cast_sink})));
  mock_timer_->Fire();
}

TEST_P(CastMediaSinkServiceImplTest, TestMultipleOnChannelOpenSucceeded) {
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);
  MediaSinkInternal cast_sink3 = CreateCastSink(3);

  CastSinkExtraData extra_data = cast_sink3.cast_data();
  extra_data.discovery_type = CastDiscoveryType::kDial;
  cast_sink3.set_cast_data(extra_data);

  cast_channel::MockCastSocket socket2;
  socket2.set_id(2);
  cast_channel::MockCastSocket socket3;
  socket3.set_id(3);

  MockBoolCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(true)).Times(3);

  // Current round of Dns discovery finds service1 and service 2.
  // Fail to open channel 1.
  base::HistogramTester tester;
  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink2));
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink2, &socket2, CastMediaSinkServiceImpl::SinkSource::kMdns,
      mock_callback.Get());
  EXPECT_THAT(
      tester.GetAllSamples(
          CastDeviceCountMetrics::kHistogramCastDiscoverySinkSource),
      ElementsAre(Bucket(
          static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kMdns), 1)));

  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink3));
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink3, &socket3, CastMediaSinkServiceImpl::SinkSource::kDial,
      mock_callback.Get());
  EXPECT_THAT(
      tester.GetAllSamples(
          CastDeviceCountMetrics::kHistogramCastDiscoverySinkSource),
      ElementsAre(
          Bucket(static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kMdns),
                 1),
          Bucket(static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kDial),
                 1)));

  extra_data.discovery_type = CastDiscoveryType::kMdns;
  cast_sink3.set_cast_data(extra_data);
  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink3));
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink3, &socket3, CastMediaSinkServiceImpl::SinkSource::kMdns,
      mock_callback.Get());
  EXPECT_THAT(
      tester.GetAllSamples(
          CastDeviceCountMetrics::kHistogramCastDiscoverySinkSource),
      ElementsAre(
          Bucket(static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kMdns),
                 1),
          Bucket(static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kDial),
                 1),
          Bucket(
              static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kDialMdns),
              1)));

  // Verify sink content
  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_CALL(mock_sink_discovered_cb_,
              Run(std::vector<MediaSinkInternal>({cast_sink2, cast_sink3})));
  mock_timer_->Fire();
}

TEST_P(CastMediaSinkServiceImplTest, TestTimer) {
  auto cast_sink1 = CreateCastSink(1);
  auto cast_sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);

  EXPECT_FALSE(mock_timer_->IsRunning());
  media_sink_service_impl_.Start();

  // Channel 2 is opened.
  cast_channel::MockCastSocket socket2;
  socket2.set_id(2);

  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink2));
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink2, &socket2, CastMediaSinkServiceImpl::SinkSource::kMdns,
      base::DoNothing());

  std::vector<MediaSinkInternal> sinks;
  EXPECT_CALL(mock_sink_discovered_cb_, Run(_)).WillOnce(SaveArg<0>(&sinks));

  // Fire timer.
  mock_timer_->Fire();
  EXPECT_EQ(sinks, std::vector<MediaSinkInternal>({cast_sink2}));

  EXPECT_FALSE(mock_timer_->IsRunning());
  // Channel 1 is opened and timer is restarted.
  cast_channel::MockCastSocket socket1;
  socket1.set_id(1);

  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink1));
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink1, &socket1, CastMediaSinkServiceImpl::SinkSource::kMdns,
      base::DoNothing());
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_P(CastMediaSinkServiceImplTest, TestOpenChannelNoRetry) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint);
  socket.SetErrorState(cast_channel::ChannelError::NONE);

  // No pending sink
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint, _)).Times(1);
  media_sink_service_impl_.OpenChannel(
      cast_sink, nullptr, CastMediaSinkServiceImpl::SinkSource::kMdns,
      base::DoNothing(),
      media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink));

  // One pending sink, the same as |cast_sink|
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint, _)).Times(0);
  media_sink_service_impl_.OpenChannel(
      cast_sink, nullptr, CastMediaSinkServiceImpl::SinkSource::kMdns,
      base::DoNothing(),
      media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink));
}

TEST_P(CastMediaSinkServiceImplTest, TestOpenChannelRetryOnce) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint);
  socket.SetErrorState(cast_channel::ChannelError::NONE);
  socket.SetErrorState(cast_channel::ChannelError::CAST_SOCKET_ERROR);

  std::unique_ptr<net::BackoffEntry> backoff_entry(
      new net::BackoffEntry(&media_sink_service_impl_.backoff_policy_));
  media_sink_service_impl_.retry_params_.max_retry_attempts = 3;
  ExpectOpenSocket(&socket);

  MockBoolCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(true));

  media_sink_service_impl_.OpenChannel(
      cast_sink, std::move(backoff_entry),
      CastMediaSinkServiceImpl::SinkSource::kMdns, mock_callback.Get(),
      media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink));

  socket.SetErrorState(cast_channel::ChannelError::NONE);
  ExpectOpenSocket(&socket);
  // Wait for 16 seconds.
  mock_time_task_runner_->FastForwardBy(base::Seconds(16));
}

TEST_P(CastMediaSinkServiceImplTest, TestOpenChannelFails) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  const net::IPEndPoint& ip_endpoint = cast_sink.cast_data().ip_endpoint;
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint);
  socket.SetErrorState(cast_channel::ChannelError::CAST_SOCKET_ERROR);

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint, _))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(&socket));
  media_sink_service_impl_.OpenChannel(
      cast_sink, nullptr, CastMediaSinkServiceImpl::SinkSource::kMdns,
      base::DoNothing(),
      media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink));

  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(4,
            media_sink_service_impl_.failure_count_map_[cast_sink.sink().id()]);
}

TEST_P(CastMediaSinkServiceImplTest, TestMultipleOpenChannels) {
  auto cast_sink1 = CreateCastSink(1);
  auto cast_sink2 = CreateCastSink(2);
  auto cast_sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);

  base::SimpleTestClock clock;
  base::Time start_time = base::Time::Now();
  clock.SetNow(start_time);
  media_sink_service_impl_.SetClockForTest(&clock);

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _));
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint2, _));

  // 1st round finds service 1 & 2.
  std::vector<MediaSinkInternal> sinks1{cast_sink1, cast_sink2};
  media_sink_service_impl_.OpenChannels(
      sinks1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Channel 2 opened.
  cast_channel::MockCastSocket socket2;
  socket2.set_id(2);
  socket2.SetErrorState(cast_channel::ChannelError::NONE);

  base::TimeDelta delta = base::Seconds(2);
  clock.Advance(delta);
  base::HistogramTester tester;

  MockBoolCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(true)).Times(3);

  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink2));
  media_sink_service_impl_.OnChannelOpened(
      cast_sink2, nullptr, CastMediaSinkServiceImpl::SinkSource::kMdns,
      start_time, mock_callback.Get(),
      media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink2),
      &socket2);
  tester.ExpectUniqueSample(CastAnalytics::kHistogramCastMdnsChannelOpenSuccess,
                            delta.InMilliseconds(), 1);

  // There is already a socket open for |ip_endpoint2|.
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint2, _))
      .Times(0);
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint3, _));

  // 2nd round finds service 2 & 3.
  std::vector<MediaSinkInternal> sinks2{cast_sink2, cast_sink3};
  media_sink_service_impl_.OpenChannels(
      sinks2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Channel 1 and 3 opened.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket3;
  socket1.set_id(1);
  socket3.set_id(3);
  socket1.SetErrorState(cast_channel::ChannelError::NONE);
  socket3.SetErrorState(cast_channel::ChannelError::NONE);
  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink1));
  media_sink_service_impl_.OnChannelOpened(
      cast_sink1, nullptr, CastMediaSinkServiceImpl::SinkSource::kMdns,
      start_time, mock_callback.Get(),
      media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink1),
      &socket1);
  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink3));
  media_sink_service_impl_.OnChannelOpened(
      cast_sink3, nullptr, CastMediaSinkServiceImpl::SinkSource::kMdns,
      start_time, mock_callback.Get(),
      media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink3),
      &socket3);

  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_CALL(mock_sink_discovered_cb_,
              Run(std::vector<MediaSinkInternal>(
                  {cast_sink1, cast_sink2, cast_sink3})));
  mock_timer_->Fire();
}

TEST_P(CastMediaSinkServiceImplTest, OpenChannelNewIPSameSink) {
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = cast_sink1.cast_data().ip_endpoint;

  cast_channel::MockCastSocket socket;
  socket.set_id(1);

  base::SimpleTestClock clock;
  base::Time start_time = base::Time::Now();
  clock.SetNow(start_time);
  media_sink_service_impl_.SetClockForTest(&clock);

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(&socket));
  std::vector<MediaSinkInternal> sinks1 = {cast_sink1};
  media_sink_service_impl_.OpenChannels(
      sinks1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1u, media_sink_service_impl_.GetSinks().size());

  // |cast_sink1| changed IP address and is discovered by mdns before it is
  // removed from |media_sink_service_impl_| first.
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  CastSinkExtraData extra_data = cast_sink1.cast_data();
  extra_data.ip_endpoint = ip_endpoint2;
  cast_sink1.set_cast_data(extra_data);

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint2, _))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(&socket));

  std::vector<MediaSinkInternal> updated_sinks1 = {cast_sink1};
  media_sink_service_impl_.OpenChannels(
      updated_sinks1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // The entry under old IPEndPoint is removed and replaced with new IPEndPoint.
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  const auto& current_sinks = media_sink_service_impl_.GetSinks();
  EXPECT_EQ(1u, current_sinks.size());
  auto sink_it = current_sinks.find(cast_sink1.sink().id());
  ASSERT_TRUE(sink_it != current_sinks.end());
  EXPECT_EQ(cast_sink1, sink_it->second);
}

TEST_P(CastMediaSinkServiceImplTest, OpenChannelUpdatedSinkSameIP) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint = cast_sink.cast_data().ip_endpoint;

  cast_channel::MockCastSocket socket;
  socket.set_id(1);

  base::SimpleTestClock clock;
  base::Time start_time = base::Time::Now();
  clock.SetNow(start_time);
  media_sink_service_impl_.SetClockForTest(&clock);

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint, _))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(&socket));
  std::vector<MediaSinkInternal> sinks = {cast_sink};
  OpenChannels(sinks, CastMediaSinkServiceImpl::SinkSource::kMdns);

  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1u, media_sink_service_impl_.GetSinks().size());

  cast_sink.sink().set_name("Updated name");
  std::vector<MediaSinkInternal> updated_sinks = {cast_sink};

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(_, _)).Times(0);
  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink));
  OpenChannels(updated_sinks, CastMediaSinkServiceImpl::SinkSource::kMdns);

  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  const auto& current_sinks = media_sink_service_impl_.GetSinks();
  EXPECT_EQ(1u, current_sinks.size());
  auto sink_it = current_sinks.find(cast_sink.sink().id());
  ASSERT_TRUE(sink_it != current_sinks.end());
  EXPECT_EQ(cast_sink, sink_it->second);
}

TEST_P(CastMediaSinkServiceImplTest, TestOnChannelOpenFailed) {
  auto cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint1);

  auto cast_sink2 = CreateCastSink(2);

  MockBoolCallback mock_callback_true;
  EXPECT_CALL(mock_callback_true, Run(true)).Times(1);

  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink));
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink, &socket, CastMediaSinkServiceImpl::SinkSource::kMdns,
      mock_callback_true.Get());

  EXPECT_EQ(1u, media_sink_service_impl_.GetSinks().size());

  MockBoolCallback mock_callback_false;
  EXPECT_CALL(mock_callback_false, Run(false)).Times(2);

  // OnChannelOpenFailed called with mismatched sink: no-op.
  EXPECT_CALL(observer_, OnSinkRemoved(_)).Times(0);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1, cast_sink2,
                                               mock_callback_false.Get());
  EXPECT_FALSE(media_sink_service_impl_.GetSinks().empty());

  EXPECT_CALL(observer_, OnSinkRemoved(cast_sink));
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1, cast_sink,
                                               mock_callback_false.Get());
  EXPECT_TRUE(media_sink_service_impl_.GetSinks().empty());
}

TEST_P(CastMediaSinkServiceImplTest, TestSuccessOnChannelErrorRetry) {
  auto cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint1);
  EXPECT_CALL(socket, ready_state())
      .WillOnce(Return(cast_channel::ReadyState::OPEN));

  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink));
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink, &socket, CastMediaSinkServiceImpl::SinkSource::kMdns,
      base::DoNothing());

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(&socket));
  media_sink_service_impl_.OnError(socket,
                                   cast_channel::ChannelError::PING_TIMEOUT);

  // Retry succeeds and the sink stays around.
  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink));
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1u, media_sink_service_impl_.GetSinks().size());
}

TEST_P(CastMediaSinkServiceImplTest, TestFailureOnChannelErrorRetry) {
  auto cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint1);
  EXPECT_CALL(socket, ready_state())
      .WillOnce(Return(cast_channel::ReadyState::OPEN));

  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink));
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink, &socket, CastMediaSinkServiceImpl::SinkSource::kMdns,
      base::DoNothing());

  // Set the error state to indicate that opening a channel failed.
  socket.SetErrorState(ChannelError::CONNECT_ERROR);
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(&socket));
  media_sink_service_impl_.OnError(socket,
                                   cast_channel::ChannelError::PING_TIMEOUT);

  // After failed attempts, the sink is removed.
  EXPECT_CALL(observer_, OnSinkRemoved(cast_sink));
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(media_sink_service_impl_.GetSinks().empty());
}

TEST_P(CastMediaSinkServiceImplTest,
       TestOnChannelErrorMayRetryForConnectingChannel) {
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  media_sink_service_impl_.AddOrUpdateSink(cast_sink1);

  net::IPEndPoint ip_endpoint1 = cast_sink1.cast_data().ip_endpoint;
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint1);

  // No op for CONNECTING cast channel.
  EXPECT_CALL(socket, ready_state())
      .WillOnce(Return(cast_channel::ReadyState::CONNECTING));
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(_, _)).Times(0);

  base::HistogramTester tester;
  media_sink_service_impl_.OnError(
      socket, cast_channel::ChannelError::CHANNEL_NOT_OPEN);

  tester.ExpectTotalCount(CastAnalytics::kHistogramCastChannelError, 1);
  EXPECT_THAT(tester.GetAllSamples(CastAnalytics::kHistogramCastChannelError),
              ElementsAre(Bucket(
                  static_cast<int>(MediaRouterChannelError::UNKNOWN), 1)));
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_P(CastMediaSinkServiceImplTest, TestOnChannelErrorNoRetryForMissingSink) {
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint1);

  media_sink_service_impl_.OnError(
      socket, cast_channel::ChannelError::CHANNEL_NOT_OPEN);

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _))
      .Times(0);
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_P(CastMediaSinkServiceImplTest, TestOnSinkAddedOrUpdated) {
  // If the DialMediaSinkService is not enabled, bypass this test.
  if (!GetParam()) {
    return;
  }

  // Make sure |media_sink_service_impl_| adds itself as an observer to
  // |dial_media_sink_service_|.
  media_sink_service_impl_.Start();

  MediaSinkInternal dial_sink1 = CreateDialSink(1);
  MediaSinkInternal dial_sink2 = CreateDialSink(2);
  net::IPEndPoint ip_endpoint1(dial_sink1.dial_data().ip_address,
                               kCastControlPort);
  net::IPEndPoint ip_endpoint2(dial_sink2.dial_data().ip_address,
                               kCastControlPort);

  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.set_id(1);
  socket2.set_id(2);
  socket2.SetAudioOnly(true);

  // Channel 1, 2 opened.
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _))
      .WillOnce(base::test::RunOnceCallback<1>(&socket1));
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint2, _))
      .WillOnce(base::test::RunOnceCallback<1>(&socket2));

  // Add DIAL sinks to |dial_media_sink_service_|, which in turn notifies
  // |media_sink_service_impl_| via the Observer interface.
  dial_media_sink_service_.AddOrUpdateSink(dial_sink1);
  dial_media_sink_service_.AddOrUpdateSink(dial_sink2);
  EXPECT_TRUE(dial_media_sink_service_.timer()->IsRunning());

  // Verify sink content.
  const auto& sinks = media_sink_service_impl_.GetSinks();
  EXPECT_EQ(2u, sinks.size());

  const MediaSinkInternal* sink = media_sink_service_impl_.GetSinkById(
      CastMediaSinkServiceImpl::GetCastSinkIdFromDial(dial_sink1.sink().id()));
  ASSERT_TRUE(sink);
  EXPECT_EQ(SinkIconType::CAST, sink->sink().icon_type());

  sink = media_sink_service_impl_.GetSinkById(
      CastMediaSinkServiceImpl::GetCastSinkIdFromDial(dial_sink2.sink().id()));
  ASSERT_TRUE(sink);
  EXPECT_EQ(SinkIconType::CAST_AUDIO, sink->sink().icon_type());

  // The sinks are removed from |dial_media_sink_service_|.
  EXPECT_TRUE(dial_media_sink_service_.GetSinks().empty());
}

TEST_P(CastMediaSinkServiceImplTest,
       TestOnSinkAddedOrUpdatedSkipsIfNonCastDevice) {
  MediaSinkInternal dial_sink1 = CreateDialSink(1);
  net::IPEndPoint ip_endpoint1(dial_sink1.dial_data().ip_address,
                               kCastControlPort);

  cast_channel::MockCastSocket socket1;
  socket1.set_id(1);
  socket1.SetErrorState(cast_channel::ChannelError::CONNECT_ERROR);

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _))
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<1>(&socket1));
  media_sink_service_impl_.OnSinkAddedOrUpdated(dial_sink1);

  // We don't trigger retries, thus each iteration will only increment the
  // failure count once.
  for (int i = 0; i < CastMediaSinkServiceImpl::kMaxDialSinkFailureCount - 1;
       ++i) {
    EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _))
        .Times(1)
        .WillOnce(base::test::RunOnceCallback<1>(&socket1));
    media_sink_service_impl_.OnSinkAddedOrUpdated(dial_sink1);
  }

  // OnChannelOpenFailed too many times; next time OnSinkAddedOrUpdated is
  // called, we won't attempt to open channel.
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _))
      .Times(0);

  media_sink_service_impl_.OnSinkAddedOrUpdated(dial_sink1);

  EXPECT_TRUE(media_sink_service_impl_.GetSinks().empty());

  // Same IP address as dial_sink1; thus it is considered to be the same device.
  // The outcome of the channel does not matter here; the sink is considered a
  // Cast device since it has been discovered via mDNS.
  MediaSinkInternal cast_sink = CreateCastSink(1);
  std::vector<MediaSinkInternal> cast_sinks = {cast_sink};
  ASSERT_EQ(ip_endpoint1.address(),
            cast_sink.cast_data().ip_endpoint.address());
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocket_(cast_sink.cast_data().ip_endpoint, _))
      .Times(1);
  media_sink_service_impl_.OpenChannels(
      cast_sinks, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // |dial_sink_failure_count| gets cleared on network change.
  media_sink_service_impl_.OnNetworksChanged("anotherNetworkId");
  EXPECT_TRUE(media_sink_service_impl_.dial_sink_failure_count_.empty());
}

TEST_P(CastMediaSinkServiceImplTest, IgnoreDialSinkIfSameIdAsCast) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal dial_sink = CreateDialSink(1);
  ASSERT_EQ(cast_sink.id(),
            CastMediaSinkServiceImpl::GetCastSinkIdFromDial(dial_sink.id()));

  media_sink_service_impl_.AddOrUpdateSink(cast_sink);

  // Since there already exists a sink with the same ID, we should not try to
  // open a channel again.
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(_, _)).Times(0);
  media_sink_service_impl_.OnSinkAddedOrUpdated(dial_sink);
}

TEST_P(CastMediaSinkServiceImplTest, IgnoreDialSinkIfSameIpAddressAsCast) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  media_sink_service_impl_.AddOrUpdateSink(cast_sink);

  // Create a DIAL sink whose ID is different from that of |cast_sink| but the
  // IP address is the same.
  MediaSinkInternal dial_sink = CreateDialSink(2);
  media_router::DialSinkExtraData extra_data = dial_sink.dial_data();
  extra_data.ip_address = cast_sink.cast_data().ip_endpoint.address();
  dial_sink.set_dial_data(extra_data);

  // Since there already exists a sink with the same IP address, we should
  // not try to open a channel again.
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(_, _)).Times(0);
  media_sink_service_impl_.OnSinkAddedOrUpdated(dial_sink);
}

TEST_P(CastMediaSinkServiceImplTest, OpenChannelsNow) {
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);
  const net::IPEndPoint& ip_endpoint1 = cast_sink1.cast_data().ip_endpoint;
  const net::IPEndPoint& ip_endpoint2 = cast_sink2.cast_data().ip_endpoint;

  // Find Cast sink 1
  media_sink_service_impl_.AddOrUpdateSink(cast_sink1);

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _))
      .Times(0);
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint2, _));

  // Attempt to connect to |cast_sink2| only since |cast_sink1| is already
  // connected.
  std::vector<MediaSinkInternal> sinks{cast_sink1, cast_sink2};
  media_sink_service_impl_.OpenChannelsNow(sinks);
}

TEST_P(CastMediaSinkServiceImplTest, CacheSinksForKnownNetwork) {
  media_sink_service_impl_.Start();
  content::RunAllTasksUntilIdle();
  // We need to run the mock task runner for the network change callback, but
  // the socket retries interfere with our normal expectations.  Instead we
  // disable retries with this line.
  media_sink_service_impl_.retry_params_.max_retry_attempts = 0;

  MediaSinkInternal sink1 = CreateCastSink(1);
  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list1{sink1, sink2};

  // Resolution will succeed for both sinks.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocket(&socket1);
  ExpectOpenSocket(&socket2);
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1, sink1,
                                               base::DoNothing());
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2, sink2,
                                               base::DoNothing());

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocket(&socket3);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _));
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint2, _));
  fake_network_info_ = fake_ethernet_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_P(CastMediaSinkServiceImplTest, CacheContainsOnlyResolvedSinks) {
  media_sink_service_impl_.Start();
  content::RunAllTasksUntilIdle();
  // We need to run the mock task runner for the network change callback, but
  // the socket retries interfere with our normal expectations.  Instead we
  // disable retries with this line.
  media_sink_service_impl_.retry_params_.max_retry_attempts = 0;

  MediaSinkInternal sink1 = CreateCastSink(1);
  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list1{sink1, sink2};

  // Resolution will fail for |sink2|.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetErrorState(cast_channel::ChannelError::CONNECT_ERROR);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocket(&socket1);
  ExpectOpenSocket(&socket2);
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1, sink1,
                                               base::DoNothing());

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocket(&socket3);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should restore only |sink1|,
  // since |sink2| failed to resolve.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _));
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint2, _))
      .Times(0);
  fake_network_info_ = fake_ethernet_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_P(CastMediaSinkServiceImplTest, CacheUpdatedOnChannelOpenFailed) {
  media_sink_service_impl_.Start();
  content::RunAllTasksUntilIdle();
  // We need to run the mock task runner for the network change callback, but
  // the socket retries interfere with our normal expectations.  Instead we
  // disable retries with this line.
  media_sink_service_impl_.retry_params_.max_retry_attempts = 0;

  MediaSinkInternal sink1 = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  std::vector<MediaSinkInternal> sink_list1{sink1};

  // Resolve |sink1| but then raise a channel error.  This should remove it from
  // the cached sinks for the ethernet network.
  cast_channel::MockCastSocket socket1;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  ExpectOpenSocket(&socket1);
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1, sink1,
                                               base::DoNothing());

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list2{sink2};

  cast_channel::MockCastSocket socket2;
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocket(&socket2);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should not restore any sinks
  // since the only sink to resolve successfully, |sink1|, later had a channel
  // error.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(_, _)).Times(0);
  fake_network_info_ = fake_ethernet_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_P(CastMediaSinkServiceImplTest, UnknownNetworkNoCache) {
  media_sink_service_impl_.Start();
  content::RunAllTasksUntilIdle();
  // We need to run the mock task runner for the network change callback, but
  // the socket retries interfere with our normal expectations.  Instead we
  // disable retries with this line.
  media_sink_service_impl_.retry_params_.max_retry_attempts = 0;

  fake_network_info_ = fake_unknown_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  MediaSinkInternal sink1 = CreateCastSink(1);
  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list1{sink1, sink2};

  // Resolution will succeed for both sinks.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocket(&socket1);
  ExpectOpenSocket(&socket2);
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Network is reported as disconnected but discover a new device.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1, sink1,
                                               base::DoNothing());
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2, sink2,
                                               base::DoNothing());

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocket(&socket3);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connecting to a network whose ID resolves to __unknown__ shouldn't pull any
  // cache items from another unknown network.
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(_, _)).Times(0);
  fake_network_info_ = fake_unknown_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  // Similarly, disconnecting from the network shouldn't pull any cache items.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_P(CastMediaSinkServiceImplTest, CacheUpdatedForKnownNetwork) {
  media_sink_service_impl_.Start();
  content::RunAllTasksUntilIdle();
  // We need to run the mock task runner for the network change callback, but
  // the socket retries interfere with our normal expectations.  Instead we
  // disable retries with this line.
  media_sink_service_impl_.retry_params_.max_retry_attempts = 0;

  MediaSinkInternal sink1 = CreateCastSink(1);
  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list1{sink1, sink2};

  // Resolution will succeed for both sinks.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocket(&socket1);
  ExpectOpenSocket(&socket2);
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1, sink1,
                                               base::DoNothing());
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2, sink2,
                                               base::DoNothing());

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocket(&socket3);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.  |sink3| is also lost.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint3, sink3,
                                               base::DoNothing());

  // Resolution will fail for cached sinks.
  socket1.SetErrorState(cast_channel::ChannelError::CONNECT_ERROR);
  socket2.SetErrorState(cast_channel::ChannelError::CONNECT_ERROR);
  ExpectOpenSocket(&socket1);
  ExpectOpenSocket(&socket2);
  fake_network_info_ = fake_ethernet_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  // A new sink is found on the ethernet network.
  MediaSinkInternal sink4 = CreateCastSink(4);
  net::IPEndPoint ip_endpoint4 = CreateIPEndPoint(4);
  std::vector<MediaSinkInternal> sink_list3{sink4};

  cast_channel::MockCastSocket socket4;
  socket4.SetIPEndpoint(ip_endpoint4);
  socket4.set_id(4);
  ExpectOpenSocket(&socket4);
  media_sink_service_impl_.OpenChannels(
      sink_list3, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Disconnect from the network and lose sinks.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint4, sink4,
                                               base::DoNothing());

  // Reconnect and expect only |sink4| to be cached.
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint4, _));
  fake_network_info_ = fake_ethernet_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_P(CastMediaSinkServiceImplTest, CacheDialDiscoveredSinks) {
  media_sink_service_impl_.Start();
  content::RunAllTasksUntilIdle();
  // We need to run the mock task runner for the network change callback, but
  // the socket retries interfere with our normal expectations.  Instead we
  // disable retries with this line.
  media_sink_service_impl_.retry_params_.max_retry_attempts = 0;

  MediaSinkInternal sink1_cast = CreateCastSink(1);
  MediaSinkInternal sink2_dial = CreateDialSink(2);
  const net::IPEndPoint& ip_endpoint1 = sink1_cast.cast_data().ip_endpoint;
  net::IPEndPoint ip_endpoint2(sink2_dial.dial_data().ip_address,
                               kCastControlPort);
  std::vector<MediaSinkInternal> sink_list1{sink1_cast};

  // Resolution will succeed for both sinks.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocket(&socket1);
  ExpectOpenSocket(&socket2);
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);
  media_sink_service_impl_.OnSinkAddedOrUpdated(sink2_dial);

  // CastMediaSinkServiceImpl generates a Cast sink based on |sink2_dial|.
  const auto& sinks = media_sink_service_impl_.GetSinks();
  auto sink2_it = base::ranges::find(
      sinks, ip_endpoint2,
      [](const auto& entry) { return entry.second.cast_data().ip_endpoint; });
  ASSERT_TRUE(sink2_it != sinks.end());
  MediaSinkInternal sink2_cast_from_dial = sink2_it->second;

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1, sink1_cast,
                                               base::DoNothing());
  media_sink_service_impl_.OnChannelOpenFailed(
      ip_endpoint2, sink2_cast_from_dial, base::DoNothing());

  MediaSinkInternal sink3_cast = CreateCastSink(3);
  MediaSinkInternal sink4_dial = CreateDialSink(4);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  net::IPEndPoint ip_endpoint4(sink4_dial.dial_data().ip_address,
                               kCastControlPort);
  std::vector<MediaSinkInternal> sink_list2{sink3_cast};

  cast_channel::MockCastSocket socket3;
  cast_channel::MockCastSocket socket4;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  socket4.SetIPEndpoint(ip_endpoint4);
  socket4.set_id(4);
  ExpectOpenSocket(&socket3);
  ExpectOpenSocket(&socket4);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);
  media_sink_service_impl_.OnSinkAddedOrUpdated(sink4_dial);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _));
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint2, _));
  fake_network_info_ = fake_ethernet_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_P(CastMediaSinkServiceImplTest, DualDiscoveryDoesntDuplicateCacheItems) {
  media_sink_service_impl_.Start();
  content::RunAllTasksUntilIdle();
  // We need to run the mock task runner for the network change callback, but
  // the socket retries interfere with our normal expectations.  Instead we
  // disable retries with this line.
  media_sink_service_impl_.retry_params_.max_retry_attempts = 0;

  // The same sink will be discovered via dial and mdns.
  MediaSinkInternal sink1_cast = CreateCastSink(0);
  MediaSinkInternal sink1_dial = CreateDialSink(0);
  net::IPEndPoint ip_endpoint1_cast = CreateIPEndPoint(0);
  net::IPEndPoint ip_endpoint1_dial(sink1_dial.dial_data().ip_address,
                                    kCastControlPort);
  std::vector<MediaSinkInternal> sink_list1{sink1_cast};

  // Dial discovery will succeed first.
  cast_channel::MockCastSocket socket1_dial;
  socket1_dial.SetIPEndpoint(ip_endpoint1_dial);
  socket1_dial.set_id(1);
  ExpectOpenSocket(&socket1_dial);
  media_sink_service_impl_.OnSinkAddedOrUpdated(sink1_dial);

  // The same sink is then discovered via mdns. However we won't open channel
  // again.
  cast_channel::MockCastSocket socket1_cast;
  socket1_cast.SetIPEndpoint(ip_endpoint1_cast);
  socket1_cast.set_id(2);

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1_cast, _))
      .Times(0);
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1_cast, sink1_cast,
                                               base::DoNothing());
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1_dial, sink1_dial,
                                               base::DoNothing());

  MediaSinkInternal sink2_cast = CreateCastSink(2);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list2{sink2_cast};

  cast_channel::MockCastSocket socket2;
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(3);
  ExpectOpenSocket(&socket2);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1_cast, _));
  fake_network_info_ = fake_ethernet_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_P(CastMediaSinkServiceImplTest, CacheSinksForDirectNetworkChange) {
  media_sink_service_impl_.Start();
  content::RunAllTasksUntilIdle();
  // We need to run the mock task runner for the network change callback, but
  // the socket retries interfere with our normal expectations.  Instead we
  // disable retries with this line.
  media_sink_service_impl_.retry_params_.max_retry_attempts = 0;

  MediaSinkInternal sink1 = CreateCastSink(1);
  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list1{sink1, sink2};

  // Resolution will succeed for both sinks.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocket(&socket1);
  ExpectOpenSocket(&socket2);
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connect to a new network with different sinks.
  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1, sink1,
                                               base::DoNothing());
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2, sink2,
                                               base::DoNothing());

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocket(&socket3);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _));
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint2, _));
  fake_network_info_ = fake_ethernet_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_P(CastMediaSinkServiceImplTest, TestCreateCastSocketOpenParams) {
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  const MediaSink::Id& sink_id = cast_sink1.sink().id();
  int connect_timeout_in_seconds =
      media_sink_service_impl_.open_params_.connect_timeout_in_seconds;
  int liveness_timeout_in_seconds =
      media_sink_service_impl_.open_params_.liveness_timeout_in_seconds;
  int delta_in_seconds = 5;
  media_sink_service_impl_.open_params_.dynamic_timeout_delta_in_seconds =
      delta_in_seconds;

  // No error
  auto open_params =
      media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink1);
  EXPECT_EQ(connect_timeout_in_seconds,
            open_params.connect_timeout.InSeconds());
  EXPECT_EQ(liveness_timeout_in_seconds,
            open_params.liveness_timeout.InSeconds());

  // One error
  connect_timeout_in_seconds += delta_in_seconds;
  liveness_timeout_in_seconds += delta_in_seconds;
  media_sink_service_impl_.failure_count_map_[sink_id] = 1;
  open_params = media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink1);
  EXPECT_EQ(connect_timeout_in_seconds,
            open_params.connect_timeout.InSeconds());
  EXPECT_EQ(liveness_timeout_in_seconds,
            open_params.liveness_timeout.InSeconds());

  // Two errors
  connect_timeout_in_seconds += delta_in_seconds;
  liveness_timeout_in_seconds += delta_in_seconds;
  media_sink_service_impl_.failure_count_map_[sink_id] = 2;
  open_params = media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink1);
  EXPECT_EQ(connect_timeout_in_seconds,
            open_params.connect_timeout.InSeconds());
  EXPECT_EQ(liveness_timeout_in_seconds,
            open_params.liveness_timeout.InSeconds());

  // Ten errors
  connect_timeout_in_seconds = 30;
  liveness_timeout_in_seconds = 60;
  media_sink_service_impl_.failure_count_map_[sink_id] = 10;
  open_params = media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink1);
  EXPECT_EQ(connect_timeout_in_seconds,
            open_params.connect_timeout.InSeconds());
  EXPECT_EQ(liveness_timeout_in_seconds,
            open_params.liveness_timeout.InSeconds());
}

TEST_P(CastMediaSinkServiceImplTest, TestHasSink) {
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);

  media_sink_service_impl_.AddOrUpdateSink(cast_sink1);
  EXPECT_TRUE(media_sink_service_impl_.HasSink(cast_sink1.id()));
  EXPECT_FALSE(media_sink_service_impl_.HasSink(cast_sink2.id()));
}

TEST_P(CastMediaSinkServiceImplTest, TestDisconnectAndRemoveSink) {
  auto cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint);
  socket.SetErrorState(cast_channel::ChannelError::NONE);

  ExpectOpenSocket(&socket);
  media_sink_service_impl_.OpenChannel(
      cast_sink, nullptr, CastMediaSinkServiceImpl::SinkSource::kAccessCode,
      base::DoNothing(),
      media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink));

  // Simulate channel is successfully opened.
  EXPECT_CALL(observer_, OnSinkAddedOrUpdated(cast_sink));
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink, &socket, CastMediaSinkServiceImpl::SinkSource::kAccessCode,
      base::DoNothing());

  EXPECT_EQ(1u, media_sink_service_impl_.GetSinks().size());

  // Verify sink content
  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_CALL(mock_sink_discovered_cb_,
              Run(std::vector<MediaSinkInternal>({cast_sink})));
  mock_timer_->Fire();

  ON_CALL(*mock_cast_socket_service_,
          GetSocket(testing::Matcher<const net::IPEndPoint&>(_)))
      .WillByDefault(testing::Return(&socket));

  // Expect that the sink is removed from objects.
  EXPECT_CALL(*mock_cast_socket_service_, CloseSocket(_));
  EXPECT_CALL(observer_, OnSinkRemoved(cast_sink));
  media_sink_service_impl_.DisconnectAndRemoveSink(cast_sink);
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(media_sink_service_impl_.GetSinks().empty());
}

TEST_P(CastMediaSinkServiceImplTest, TestAccessCodeSinkNotAddedToNetworkCache) {
  media_sink_service_impl_.Start();
  content::RunAllTasksUntilIdle();
  // We need to run the mock task runner for the network change callback, but
  // the socket retries interfere with our normal expectations.  Instead we
  // disable retries with this line.
  media_sink_service_impl_.retry_params_.max_retry_attempts = 0;

  MediaSinkInternal sink1 = CreateCastSink(1);
  MediaSinkInternal access_sink = CreateCastSink(2);
  access_sink.cast_data().discovery_type =
      CastDiscoveryType::kAccessCodeManualEntry;
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list1{sink1, access_sink};

  // Resolution will succeed for both sinks.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocket(&socket1);
  ExpectOpenSocket(&socket2);
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connect to a new network with different sinks.
  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1, sink1,
                                               base::DoNothing());
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2, access_sink,
                                               base::DoNothing());

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocket(&socket3);

  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them. access_sink should not be
  // present so no OpenSocket call should not be made.
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint1, _));
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint2, _))
      .Times(0);

  fake_network_info_ = fake_ethernet_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);

  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_P(CastMediaSinkServiceImplTest,
       TestOpenChannelFailsForPubliclyRoutableIP) {
  MediaSinkInternal cast_sink = CreateCastSink(1);

  net::IPAddress address;
  EXPECT_TRUE(address.AssignFromIPLiteral(kPubliclyRoutableIPv4Address));
  ASSERT_TRUE(address.IsValid());

  auto ip_endpoint = net::IPEndPoint(address, 8009);
  ASSERT_TRUE(ip_endpoint.address().IsPubliclyRoutable());

  CastSinkExtraData extra_data = cast_sink.cast_data();
  extra_data.ip_endpoint = ip_endpoint;
  cast_sink.set_cast_data(extra_data);

  MockBoolCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(false)).Times(1);

  // No pending sink
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocket_(ip_endpoint, _)).Times(0);
  media_sink_service_impl_.OpenChannel(
      cast_sink, nullptr, CastMediaSinkServiceImpl::SinkSource::kMdns,
      mock_callback.Get(),
      media_sink_service_impl_.CreateCastSocketOpenParams(cast_sink));
}

INSTANTIATE_TEST_SUITE_P(DialMediaSinkServiceEnabled,
                         CastMediaSinkServiceImplTest,
                         testing::Bool());

}  // namespace media_router

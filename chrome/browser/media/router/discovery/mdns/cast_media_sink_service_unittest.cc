// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/test/mock_dns_sd_registry.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;
using cast_channel::CastDeviceCapability;

namespace {

net::IPEndPoint CreateIPEndPoint(int num) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(
      base::StringPrintf("192.168.0.10%d", num)));
  return net::IPEndPoint(ip_address, 8009 + num);
}

media_router::DnsSdService CreateDnsService(int num, int capabilities) {
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(num);
  media_router::DnsSdService service;
  service.service_name =
      "_myDevice." + std::string(media_router::kCastServiceType);
  service.ip_address = ip_endpoint.address().ToString();
  service.service_host_port = net::HostPortPair::FromIPEndPoint(ip_endpoint);
  service.service_data.push_back(base::StringPrintf("id=service %d", num));
  service.service_data.push_back(
      base::StringPrintf("fn=friendly name %d", num));
  service.service_data.push_back(base::StringPrintf("md=model name %d", num));
  service.service_data.push_back(base::StringPrintf("ca=%d", capabilities));

  return service;
}

}  // namespace

namespace media_router {

class MockCastMediaSinkServiceImpl : public CastMediaSinkServiceImpl {
 public:
  MockCastMediaSinkServiceImpl(
      const OnSinksDiscoveredCallback& callback,
      cast_channel::CastSocketService* cast_socket_service,
      DiscoveryNetworkMonitor* network_monitor,
      MediaSinkServiceBase* dial_media_sink_service)
      : CastMediaSinkServiceImpl(callback,
                                 cast_socket_service,
                                 network_monitor,
                                 dial_media_sink_service,
                                 /* allow_all_ips */ false),
        sinks_discovered_cb_(callback) {}
  ~MockCastMediaSinkServiceImpl() override {}

  void Start() override { DoStart(); }
  MOCK_METHOD0(DoStart, void());
  MOCK_METHOD2(OpenChannels,
               void(const std::vector<MediaSinkInternal>& cast_sinks,
                    CastMediaSinkServiceImpl::SinkSource sink_source));

  OnSinksDiscoveredCallback sinks_discovered_cb() {
    return sinks_discovered_cb_;
  }

 private:
  OnSinksDiscoveredCallback sinks_discovered_cb_;
};

class TestCastMediaSinkService : public CastMediaSinkService {
 public:
  TestCastMediaSinkService(cast_channel::CastSocketService* cast_socket_service,
                           DiscoveryNetworkMonitor* network_monitor)
      : cast_socket_service_(cast_socket_service),
        network_monitor_(network_monitor) {}
  ~TestCastMediaSinkService() override = default;

  std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
  CreateImpl(const OnSinksDiscoveredCallback& sinks_discovered_cb,
             MediaSinkServiceBase* dial_media_sink_service) override {
    auto mock_impl = std::unique_ptr<MockCastMediaSinkServiceImpl,
                                     base::OnTaskRunnerDeleter>(
        new MockCastMediaSinkServiceImpl(sinks_discovered_cb,
                                         cast_socket_service_, network_monitor_,
                                         dial_media_sink_service),
        base::OnTaskRunnerDeleter(cast_socket_service_->task_runner()));
    mock_impl_ = mock_impl.get();
    return mock_impl;
  }

  MockCastMediaSinkServiceImpl* mock_impl() { return mock_impl_; }

 private:
  cast_channel::CastSocketService* const cast_socket_service_;
  DiscoveryNetworkMonitor* const network_monitor_;
  MockCastMediaSinkServiceImpl* mock_impl_ = nullptr;
};

class CastMediaSinkServiceTest : public ::testing::Test {
 public:
  CastMediaSinkServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        mock_cast_socket_service_(
            new cast_channel::MockCastSocketService(task_runner_)),
        media_sink_service_(new TestCastMediaSinkService(
            mock_cast_socket_service_.get(),
            DiscoveryNetworkMonitor::GetInstance())),
        test_dns_sd_registry_(media_sink_service_.get()) {}

  void SetUp() override {
    EXPECT_CALL(test_dns_sd_registry_, AddObserver(media_sink_service_.get()));
    EXPECT_CALL(test_dns_sd_registry_, RegisterDnsSdListener(_));
    media_sink_service_->SetDnsSdRegistryForTest(&test_dns_sd_registry_);
    media_sink_service_->Start(mock_sink_discovered_ui_cb_.Get(),
                               &dial_media_sink_service_);
    mock_impl_ = media_sink_service_->mock_impl();
    ASSERT_TRUE(mock_impl_);
    EXPECT_CALL(*mock_impl_, DoStart()).WillOnce(InvokeWithoutArgs([this]() {
      EXPECT_TRUE(this->task_runner_->RunsTasksInCurrentSequence());
    }));
    task_runner_->RunUntilIdle();
  }

  void TearDown() override {
    EXPECT_CALL(test_dns_sd_registry_,
                RemoveObserver(media_sink_service_.get()));
    EXPECT_CALL(test_dns_sd_registry_, UnregisterDnsSdListener(_));
    media_sink_service_.reset();
    task_runner_->RunUntilIdle();
    task_environment_.RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  base::MockCallback<OnSinksDiscoveredCallback> mock_sink_discovered_ui_cb_;
  TestMediaSinkService dial_media_sink_service_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;

  std::unique_ptr<TestCastMediaSinkService> media_sink_service_;
  MockCastMediaSinkServiceImpl* mock_impl_ = nullptr;
  MockDnsSdRegistry test_dns_sd_registry_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaSinkServiceTest);
};

TEST_F(CastMediaSinkServiceTest, OnUserGesture) {
  EXPECT_CALL(test_dns_sd_registry_, ResetAndDiscover());
  media_sink_service_->OnUserGesture();
}

TEST_F(CastMediaSinkServiceTest, TestOnDnsSdEvent) {
  DnsSdService service1 = CreateDnsService(
      1, CastDeviceCapability::VIDEO_OUT | CastDeviceCapability::AUDIO_OUT);
  DnsSdService service2 =
      CreateDnsService(2, CastDeviceCapability::MULTIZONE_GROUP);
  DnsSdService service3 = CreateDnsService(3, CastDeviceCapability::NONE);

  // Add dns services.
  DnsSdRegistry::DnsSdServiceList service_list{service1, service2, service3};

  // Invoke CastSocketService::OpenSocket on the IO thread.
  media_sink_service_->OnDnsSdEvent(kCastServiceType, service_list);

  std::vector<MediaSinkInternal> sinks;
  EXPECT_CALL(*mock_impl_, OpenChannels(_, _)).WillOnce(SaveArg<0>(&sinks));

  // Invoke OpenChannels on |task_runner_|.
  task_runner_->RunUntilIdle();
  // Verify sink content
  ASSERT_EQ(3u, sinks.size());
  EXPECT_EQ(SinkIconType::CAST, sinks[0].sink().icon_type());
  EXPECT_EQ(SinkIconType::CAST_AUDIO_GROUP, sinks[1].sink().icon_type());
  EXPECT_EQ(SinkIconType::CAST_AUDIO, sinks[2].sink().icon_type());
}

}  // namespace media_router

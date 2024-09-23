// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_TEST_HELPERS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_TEST_HELPERS_H_

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/test/mock_dns_sd_registry.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

namespace media_router {

class MockCastMediaSinkServiceImpl : public CastMediaSinkServiceImpl {
 public:
  MockCastMediaSinkServiceImpl(
      const OnSinksDiscoveredCallback& callback,
      cast_channel::CastSocketService* cast_socket_service,
      DiscoveryNetworkMonitor* network_monitor,
      MediaSinkServiceBase* dial_media_sink_service);
  ~MockCastMediaSinkServiceImpl() override;

  void Start() override { DoStart(); }
  MOCK_METHOD(void, DoStart, (), ());
  MOCK_METHOD(void,
              OpenChannels,
              (const std::vector<MediaSinkInternal>& cast_sinks,
               CastMediaSinkServiceImpl::SinkSource sink_source),
              (override));
  MOCK_METHOD(void,
              DisconnectAndRemoveSink,
              (const MediaSinkInternal& sink),
              (override));
  MOCK_METHOD(void,
              OpenChannel,
              (const MediaSinkInternal& cast_sink,
               std::unique_ptr<net::BackoffEntry> backoff_entry,
               SinkSource sink_source,
               ChannelOpenedCallback callback,
               cast_channel::CastSocketOpenParams open_params),
              (override));
  MOCK_METHOD(bool, HasSink, (const MediaSink::Id& sink_id), (override));

  OnSinksDiscoveredCallback sinks_discovered_cb() {
    return sinks_discovered_cb_;
  }

 private:
  OnSinksDiscoveredCallback sinks_discovered_cb_;
};

class TestCastMediaSinkService : public CastMediaSinkService {
 public:
  TestCastMediaSinkService(cast_channel::CastSocketService* cast_socket_service,
                           DiscoveryNetworkMonitor* network_monitor);
  ~TestCastMediaSinkService() override;

  std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
  CreateImpl(const OnSinksDiscoveredCallback& sinks_discovered_cb,
             MediaSinkServiceBase* dial_media_sink_service) override;
  MOCK_METHOD(void, StartMdnsDiscovery, ());

  MockCastMediaSinkServiceImpl* mock_impl() { return mock_impl_; }

 private:
  const raw_ptr<cast_channel::CastSocketService> cast_socket_service_;
  const raw_ptr<DiscoveryNetworkMonitor> network_monitor_;
  raw_ptr<MockCastMediaSinkServiceImpl> mock_impl_ = nullptr;
};

}  // namespace media_router
#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_TEST_HELPERS_H_

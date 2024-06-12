// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_test_helpers.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
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

namespace media_router {

MockCastMediaSinkServiceImpl::MockCastMediaSinkServiceImpl(
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

MockCastMediaSinkServiceImpl::~MockCastMediaSinkServiceImpl() = default;

TestCastMediaSinkService::TestCastMediaSinkService(
    cast_channel::CastSocketService* cast_socket_service,
    DiscoveryNetworkMonitor* network_monitor)
    : cast_socket_service_(cast_socket_service),
      network_monitor_(network_monitor) {}

TestCastMediaSinkService::~TestCastMediaSinkService() = default;

std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
TestCastMediaSinkService::CreateImpl(
    const OnSinksDiscoveredCallback& sinks_discovered_cb,
    MediaSinkServiceBase* dial_media_sink_service) {
  auto mock_impl =
      std::unique_ptr<MockCastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>(
          new NiceMock<MockCastMediaSinkServiceImpl>(
              sinks_discovered_cb, cast_socket_service_, network_monitor_,
              dial_media_sink_service),
          base::OnTaskRunnerDeleter(cast_socket_service_->task_runner()));
  mock_impl_ = mock_impl.get();
  return mock_impl;
}

}  // namespace media_router

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_service.h"

#include <stddef.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/log/test_net_log.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using ::testing::A;
using ::testing::AtLeast;
using ::testing::Return;

namespace {

const char kValidResponse[] =
    "HTTP/1.1 OK\r\n"
    "LOCATION: http://127.0.0.1/dd.xml\r\n"
    "USN: some_id\r\n"
    "CACHE-CONTROL: max-age=1800\r\n"
    "CONFIGID.UPNP.ORG: 1\r\n\r\n";

}  // namespace

namespace media_router {

class MockObserver : public DialService::Observer {
 public:
  ~MockObserver() override {}

  MOCK_METHOD1(OnDiscoveryRequest, void(DialService*));
  MOCK_METHOD2(OnDeviceDiscovered, void(DialService*, const DialDeviceData&));
  MOCK_METHOD1(OnDiscoveryFinished, void(DialService*));
  MOCK_METHOD2(OnError,
               void(DialService*, const DialService::DialServiceErrorCode&));
};

class DialServiceTest : public testing::Test {
 public:
  DialServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        mock_ip_(net::IPAddress::IPv4AllZeros()),
        dial_service_(&test_net_log_) {
    dial_service_.AddObserver(&mock_observer_);
    dial_socket_ = dial_service_.CreateDialSocket();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  net::TestNetLog test_net_log_;
  net::IPAddress mock_ip_;
  DialServiceImpl dial_service_;
  std::unique_ptr<DialServiceImpl::DialSocket> dial_socket_;
  MockObserver mock_observer_;
};

TEST_F(DialServiceTest, TestSendMultipleRequests) {
  // Setting the finish delay to zero disables the timer that invokes
  // FinishDiscovery().
  dial_service_.finish_delay_ = TimeDelta::FromSeconds(0);
  dial_service_.request_interval_ = TimeDelta::FromSeconds(0);
  dial_service_.max_requests_ = 4;
  dial_service_.discovery_active_ = true;
  EXPECT_CALL(mock_observer_, OnDiscoveryRequest(A<DialService*>())).Times(4);
  EXPECT_CALL(mock_observer_, OnDiscoveryFinished(A<DialService*>())).Times(1);
  dial_service_.BindAndAddSocket(mock_ip_);
  EXPECT_EQ(1u, dial_service_.dial_sockets_.size());
  dial_service_.SendOneRequest();
  base::RunLoop().RunUntilIdle();
  dial_service_.FinishDiscovery();
}

TEST_F(DialServiceTest, TestMultipleNetworkInterfaces) {
  // Setting the finish delay to zero disables the timer that invokes
  // FinishDiscovery().
  dial_service_.finish_delay_ = TimeDelta::FromSeconds(0);
  dial_service_.request_interval_ = TimeDelta::FromSeconds(0);
  dial_service_.max_requests_ = 4;
  dial_service_.discovery_active_ = true;
  net::NetworkInterfaceList interface_list;
  interface_list.push_back(net::NetworkInterface(
      "network1", "network1", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      mock_ip_, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));
  interface_list.push_back(net::NetworkInterface(
      "network2", "network2", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      mock_ip_, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));
  interface_list.push_back(net::NetworkInterface(
      "network3", "network3", 2, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      mock_ip_, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  // "network4" is equivalent to "network2" because both the address family
  // and interface index are the same.
  interface_list.push_back(net::NetworkInterface(
      "network4", "network4", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      mock_ip_, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  // 3 sockets * 4 requests per socket = 12 requests
  EXPECT_CALL(mock_observer_, OnDiscoveryRequest(A<DialService*>())).Times(12);
  EXPECT_CALL(mock_observer_, OnDiscoveryFinished(A<DialService*>())).Times(1);

  dial_service_.SendNetworkList(interface_list);
  EXPECT_EQ(3u, dial_service_.dial_sockets_.size());

  base::RunLoop().RunUntilIdle();
  dial_service_.FinishDiscovery();
}

TEST_F(DialServiceTest, TestOnDiscoveryRequest) {
  dial_service_.discovery_active_ = true;
  dial_service_.num_requests_sent_ = 1;
  dial_service_.max_requests_ = 1;
  size_t num_bytes = dial_service_.send_buffer_->size();
  EXPECT_CALL(mock_observer_, OnDiscoveryRequest(A<DialService*>())).Times(1);
  dial_socket_->OnSocketWrite(num_bytes, num_bytes);
}

TEST_F(DialServiceTest, TestNotifyOnError) {
  EXPECT_CALL(mock_observer_, OnError(A<DialService*>(),
                                      DialService::DIAL_SERVICE_NO_INTERFACES));
  dial_socket_->OnSocketWrite(0, net::ERR_CONNECTION_REFUSED);
}

TEST_F(DialServiceTest, TestOnDeviceDiscovered) {
  dial_service_.discovery_active_ = true;
  int response_size = base::size(kValidResponse) - 1;
  dial_socket_->recv_buffer_ =
      base::MakeRefCounted<net::IOBufferWithSize>(response_size);
  strncpy(dial_socket_->recv_buffer_->data(), kValidResponse, response_size);
  dial_socket_->recv_address_ = net::IPEndPoint(mock_ip_, 12345);

  DialDeviceData expected_device;
  expected_device.set_device_id("some_id");

  EXPECT_CALL(mock_observer_,
              OnDeviceDiscovered(A<DialService*>(), expected_device))
      .Times(1);
  dial_socket_->OnSocketRead(response_size);
}

TEST_F(DialServiceTest, TestOnDiscoveryFinished) {
  dial_service_.discovery_active_ = true;

  EXPECT_CALL(mock_observer_, OnDiscoveryFinished(A<DialService*>())).Times(1);
  dial_service_.FinishDiscovery();
  EXPECT_FALSE(dial_service_.discovery_active_);
}

TEST_F(DialServiceTest, TestResponseParsing) {
  Time now = Time::Now();

  // Successful case
  DialDeviceData parsed;
  EXPECT_TRUE(
      DialServiceImpl::DialSocket::ParseResponse(kValidResponse, now, &parsed));
  EXPECT_EQ("some_id", parsed.device_id());
  EXPECT_EQ("http://127.0.0.1/dd.xml", parsed.device_description_url().spec());
  EXPECT_EQ(1, parsed.config_id());
  EXPECT_EQ(now, parsed.response_time());

  // Failure cases
  DialDeviceData not_parsed;

  // Empty, garbage
  EXPECT_FALSE(DialServiceImpl::DialSocket::ParseResponse(std::string(), now,
                                                          &not_parsed));
  EXPECT_FALSE(
      DialServiceImpl::DialSocket::ParseResponse("\r\n\r\n", now, &not_parsed));
  EXPECT_FALSE(
      DialServiceImpl::DialSocket::ParseResponse("xyzzy", now, &not_parsed));

  // No headers
  EXPECT_FALSE(DialServiceImpl::DialSocket::ParseResponse("HTTP/1.1 OK\r\n\r\n",
                                                          now, &not_parsed));

  // Missing LOCATION
  EXPECT_FALSE(
      DialServiceImpl::DialSocket::ParseResponse("HTTP/1.1 OK\r\n"
                                                 "USN: some_id\r\n\r\n",
                                                 now, &not_parsed));

  // Empty LOCATION
  EXPECT_FALSE(
      DialServiceImpl::DialSocket::ParseResponse("HTTP/1.1 OK\r\n"
                                                 "LOCATION:\r\n"
                                                 "USN: some_id\r\n\r\n",
                                                 now, &not_parsed));

  // Missing USN
  EXPECT_FALSE(DialServiceImpl::DialSocket::ParseResponse(
      "HTTP/1.1 OK\r\n"
      "LOCATION: http://127.0.0.1/dd.xml\r\n\r\n",
      now, &not_parsed));

  // Empty USN
  EXPECT_FALSE(DialServiceImpl::DialSocket::ParseResponse(
      "HTTP/1.1 OK\r\n"
      "LOCATION: http://127.0.0.1/dd.xml\r\n"
      "USN:\r\n\r\n",
      now, &not_parsed));
}

}  // namespace media_router

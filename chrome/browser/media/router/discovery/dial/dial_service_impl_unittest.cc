// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_service_impl.h"

#include <stddef.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"  // nogncheck
#endif

using base::Time;
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

class MockDialServiceClient : public DialService::Client {
 public:
  ~MockDialServiceClient() override = default;

  MOCK_METHOD(void, OnDiscoveryRequest, ());
  MOCK_METHOD(void, OnDeviceDiscovered, (const DialDeviceData&));
  MOCK_METHOD(void, OnDiscoveryFinished, ());
  MOCK_METHOD(void, OnError, (DialService::DialServiceErrorCode));
};

class DialServiceImplTest : public testing::Test {
 public:
  DialServiceImplTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        mock_ip_(net::IPAddress::IPv4AllZeros()),
        dial_service_(mock_client_,
                      content::GetIOThreadTaskRunner({}),
                      net::NetLog::Get()) {
    dial_socket_ = dial_service_.CreateDialSocket();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  net::IPAddress mock_ip_;
  DialServiceImpl dial_service_;
  std::unique_ptr<DialServiceImpl::DialSocket> dial_socket_;
  MockDialServiceClient mock_client_;
};

TEST_F(DialServiceImplTest, TestSendMultipleRequests) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/354933489): Investigate why this fails on MacOS 15 and
  // re-enable.
  if (base::mac::MacOSMajorVersion() >= 15) {
    return;
  }
#endif  // BUILDFLAG(IS_MAC)

  // Setting the finish delay to zero disables the timer that invokes
  // FinishDiscovery().
  dial_service_.finish_delay_ = base::Seconds(0);
  dial_service_.request_interval_ = base::Seconds(0);
  dial_service_.max_requests_ = 4;
  dial_service_.discovery_active_ = true;
  EXPECT_CALL(mock_client_, OnDiscoveryRequest()).Times(4);
  EXPECT_CALL(mock_client_, OnDiscoveryFinished()).Times(1);
  dial_service_.BindAndAddSocket(mock_ip_);
  EXPECT_EQ(1u, dial_service_.dial_sockets_.size());
  EXPECT_TRUE(dial_service_.dial_sockets_[0]);
  EXPECT_FALSE(dial_service_.dial_sockets_[0]->IsClosed());
  dial_service_.SendOneRequest();
  base::RunLoop().RunUntilIdle();
  dial_service_.FinishDiscovery();
}

TEST_F(DialServiceImplTest, TestMultipleNetworkInterfaces) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/354933489): Investigate why this fails on MacOS 15 and
  // re-enable.
  if (base::mac::MacOSMajorVersion() >= 15) {
    return;
  }
#endif  // BUILDFLAG(IS_MAC)

  // Setting the finish delay to zero disables the timer that invokes
  // FinishDiscovery().
  dial_service_.finish_delay_ = base::Seconds(0);
  dial_service_.request_interval_ = base::Seconds(0);
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
  EXPECT_CALL(mock_client_, OnDiscoveryRequest()).Times(12);
  EXPECT_CALL(mock_client_, OnDiscoveryFinished()).Times(1);

  dial_service_.SendNetworkList(interface_list);
  EXPECT_EQ(3u, dial_service_.dial_sockets_.size());

  base::RunLoop().RunUntilIdle();
  dial_service_.FinishDiscovery();
}

TEST_F(DialServiceImplTest, TestOnDiscoveryRequest) {
  dial_service_.discovery_active_ = true;
  dial_service_.num_requests_sent_ = 1;
  dial_service_.max_requests_ = 1;
  size_t num_bytes = dial_service_.send_buffer_->size();
  EXPECT_CALL(mock_client_, OnDiscoveryRequest()).Times(1);
  dial_socket_->OnSocketWrite(num_bytes, num_bytes);
}

TEST_F(DialServiceImplTest, TestNotifyOnError) {
  EXPECT_CALL(mock_client_, OnError(DialService::DIAL_SERVICE_NO_INTERFACES));
  dial_socket_->OnSocketWrite(0, net::ERR_CONNECTION_REFUSED);
}

TEST_F(DialServiceImplTest, TestOnDeviceDiscovered) {
  dial_service_.discovery_active_ = true;
  int response_size = std::size(kValidResponse) - 1;
  dial_socket_->recv_buffer_ =
      base::MakeRefCounted<net::IOBufferWithSize>(response_size);
  strncpy(dial_socket_->recv_buffer_->data(), kValidResponse, response_size);
  dial_socket_->recv_address_ =
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), 12345);

  DialDeviceData expected_device;
  expected_device.set_device_id("some_id");

  EXPECT_CALL(mock_client_, OnDeviceDiscovered(expected_device)).Times(1);
  dial_socket_->OnSocketRead(response_size);
}

TEST_F(DialServiceImplTest, TestOnDiscoveryFinished) {
  dial_service_.discovery_active_ = true;

  EXPECT_CALL(mock_client_, OnDiscoveryFinished()).Times(1);
  dial_service_.FinishDiscovery();
  EXPECT_FALSE(dial_service_.discovery_active_);
}

TEST_F(DialServiceImplTest, TestResponseParsing) {
  Time now = Time::Now();

  // Force the socket address to match what is expected in the response.
  dial_socket_->recv_address_ =
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), 12345);

  // Successful case, all values parsed successfully.
  DialDeviceData parsed;
  EXPECT_TRUE(dial_socket_->ParseResponse(kValidResponse, now, &parsed));
  EXPECT_EQ("some_id", parsed.device_id());
  EXPECT_EQ("http://127.0.0.1/dd.xml", parsed.device_description_url().spec());
  EXPECT_EQ(1, parsed.config_id());
  EXPECT_EQ(now, parsed.response_time());
  EXPECT_EQ(1800, parsed.max_age());

  // Cases where we do not fail entirely, but we are unable to extract the
  // CACHE-CONTROL or CONFIG header values.

  // Max-age is too low
  DialDeviceData parsed_max_age_low;
  EXPECT_TRUE(
      dial_socket_->ParseResponse("HTTP/1.1 OK\r\n"
                                  "LOCATION: http://127.0.0.1/dd.xml\r\n"
                                  "USN: some_id\r\n"
                                  "CACHE-CONTROL: max-age=-100\r\n"
                                  "CONFIGID.UPNP.ORG: 1\r\n\r\n",
                                  now, &parsed_max_age_low));
  EXPECT_EQ(-1, parsed_max_age_low.max_age());

  // max-age is too high
  DialDeviceData parsed_max_age_high;
  EXPECT_TRUE(
      dial_socket_->ParseResponse("HTTP/1.1 OK\r\n"
                                  "LOCATION: http://127.0.0.1/dd.xml\r\n"
                                  "USN: some_id\r\n"
                                  "CACHE-CONTROL: max-age=5000\r\n"
                                  "CONFIGID.UPNP.ORG: 1\r\n\r\n",
                                  now, &parsed_max_age_high));
  EXPECT_EQ(3600, parsed_max_age_high.max_age());

  // Invalid CACHE-CONTROL directive
  DialDeviceData parsed_invalid_cache;
  EXPECT_TRUE(
      dial_socket_->ParseResponse("HTTP/1.1 OK\r\n"
                                  "LOCATION: http://127.0.0.1/dd.xml\r\n"
                                  "USN: some_id\r\n"
                                  "CACHE-CONTROL: xyzzy=100,\r\n"
                                  "CONFIGID.UPNP.ORG: 1\r\n\r\n",
                                  now, &parsed_invalid_cache));
  EXPECT_EQ(-1, parsed_invalid_cache.max_age());

  // Extra CACHE-CONTROL directives
  DialDeviceData parsed_extra_cache;
  EXPECT_TRUE(
      dial_socket_->ParseResponse("HTTP/1.1 OK\r\n"
                                  "LOCATION: http://127.0.0.1/dd.xml\r\n"
                                  "USN: some_id\r\n"
                                  "CACHE-CONTROL: bar=a,max-age=1800,foo=b\r\n"
                                  "CONFIGID.UPNP.ORG: 1\r\n\r\n",
                                  now, &parsed_extra_cache));
  EXPECT_EQ(-1, parsed_extra_cache.max_age());

  // Invalid CONFIGID.UPNP.ORG value
  DialDeviceData parsed_invalid_config;
  EXPECT_TRUE(
      dial_socket_->ParseResponse("HTTP/1.1 OK\r\n"
                                  "LOCATION: http://127.0.0.1/dd.xml\r\n"
                                  "USN: some_id\r\n"
                                  "CACHE-CONTROL: max-age=1000\r\n"
                                  "CONFIGID.UPNP.ORG: -100\r\n\r\n",
                                  now, &parsed_invalid_config));
  EXPECT_EQ(-1, parsed_invalid_config.config_id());

  // Failure cases
  DialDeviceData not_parsed;

  // Empty, garbage
  EXPECT_FALSE(dial_socket_->ParseResponse(std::string(), now, &not_parsed));
  EXPECT_FALSE(dial_socket_->ParseResponse("\r\n\r\n", now, &not_parsed));
  EXPECT_FALSE(dial_socket_->ParseResponse("xyzzy", now, &not_parsed));

  // No headers
  EXPECT_FALSE(
      dial_socket_->ParseResponse("HTTP/1.1 OK\r\n\r\n", now, &not_parsed));

  // Missing USN
  EXPECT_FALSE(
      dial_socket_->ParseResponse("HTTP/1.1 OK\r\n"
                                  "LOCATION: http://127.0.0.1/dd.xml\r\n\r\n",
                                  now, &not_parsed));

  // Empty USN
  EXPECT_FALSE(
      dial_socket_->ParseResponse("HTTP/1.1 OK\r\n"
                                  "LOCATION: http://127.0.0.1/dd.xml\r\n"
                                  "USN:\r\n\r\n",
                                  now, &not_parsed));

  // Missing LOCATION
  EXPECT_FALSE(
      dial_socket_->ParseResponse("HTTP/1.1 OK\r\n"
                                  "USN: some_id\r\n\r\n",
                                  now, &not_parsed));

  // Empty LOCATION
  EXPECT_FALSE(
      dial_socket_->ParseResponse("HTTP/1.1 OK\r\n"
                                  "LOCATION:\r\n"
                                  "USN: some_id\r\n\r\n",
                                  now, &not_parsed));

  // Invalid LOCATION
  EXPECT_FALSE(
      dial_socket_->ParseResponse("HTTP/1.1 OK\r\n"
                                  "LOCATION: http://127.8.8.8/dd.xml\r\n"
                                  "USN:\r\n\r\n",
                                  now, &not_parsed));
}

}  // namespace media_router

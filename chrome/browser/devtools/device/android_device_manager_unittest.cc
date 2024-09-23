// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/android_device_manager.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeDeviceProvider : public AndroidDeviceManager::DeviceProvider {
 public:
  void QueryDevices(SerialsCallback callback) override {}

  void QueryDeviceInfo(const std::string& serial,
                       DeviceInfoCallback callback) override {}

  void OpenSocket(const std::string& serial,
                  const std::string& socket_name,
                  SocketCallback callback) override {
    CreateMockAcceptSocket(std::move(callback));
  }

  void ReleaseDevice(const std::string& serial) override {}

 private:
  ~FakeDeviceProvider() override = default;

  void CreateMockAcceptSocket(SocketCallback callback) {
    net::StaticSocketDataProvider provider((base::span<net::MockRead>()),
                                           base::span<net::MockWrite>());
    provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
    auto mock_socket = std::make_unique<net::MockTCPClientSocket>(
        net::AddressList(), nullptr /*netlog*/, &provider);
    mock_socket->set_enable_read_if_ready(true);
    EXPECT_EQ(net::OK, mock_socket->Connect(base::DoNothing()));
    return std::move(callback).Run(net::OK, std::move(mock_socket));
  }
};

}  // namespace

TEST(AndroidDeviceManagerTest, SendJSONRequestTimesOutOnUnresponsiveSocket) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  int recorded_result;
  scoped_refptr<FakeDeviceProvider> fake_device_provider =
      base::MakeRefCounted<FakeDeviceProvider>();
  AndroidDeviceManager::CommandCallback callback =
      base::BindOnce([](int* out_result, int result,
                        const std::string& response) { *out_result = result; },
                     &recorded_result);

  fake_device_provider->SendJsonRequest("MockSerial", "MockSocket",
                                        "MockRequest", std::move(callback));
  task_environment.FastForwardBy(base::Seconds(2));

  EXPECT_EQ(net::ERR_TIMED_OUT, recorded_result);
}

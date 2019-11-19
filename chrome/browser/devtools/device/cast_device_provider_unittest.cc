// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/cast_device_provider.h"

#include "base/bind.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "net/base/host_port_pair.h"
#include "testing/gtest/include/gtest/gtest.h"

using local_discovery::ServiceDescription;
using DeviceInfo = AndroidDeviceManager::DeviceInfo;
using BrowserInfo = AndroidDeviceManager::BrowserInfo;

namespace {

void CompareDeviceInfo(bool* was_run,
                       const DeviceInfo& expected,
                       const DeviceInfo& actual) {
  EXPECT_EQ(expected.model, actual.model);
  EXPECT_EQ(expected.connected, actual.connected);

  const BrowserInfo& exp_br_info = expected.browser_info[0];
  const BrowserInfo& act_br_info = actual.browser_info[0];
  EXPECT_EQ(exp_br_info.socket_name, act_br_info.socket_name);
  EXPECT_EQ(exp_br_info.display_name, act_br_info.display_name);
  EXPECT_EQ(exp_br_info.type, act_br_info.type);

  *was_run = true;
}

void DummyCallback(bool* was_run, const DeviceInfo& device_info) {
  *was_run = true;
}

}  // namespace

TEST(CastDeviceProviderTest, ServiceDiscovery) {
  scoped_refptr<CastDeviceProvider> device_provider_ = new CastDeviceProvider();

  // Create a cast service.
  const std::string cast_display_name = "FakeCast1337";
  const std::string cast_service_type = "_googlecast._tcp.local";
  const std::string cast_service_name = "abcdefgh";
  const std::string cast_service_model = "Fake Cast Device";

  ServiceDescription cast_service;
  cast_service.service_name = cast_service_name + "." + cast_service_type;
  cast_service.address = net::HostPortPair("192.168.1.101", 8009);
  cast_service.metadata.push_back("id=0123456789abcdef0123456789abcdef");
  cast_service.metadata.push_back("ve=00");
  cast_service.metadata.push_back("md=" + cast_service_model);
  cast_service.metadata.push_back("fn=" + cast_display_name);
  ASSERT_TRUE(cast_service.ip_address.AssignFromIPLiteral("192.168.1.101"));

  device_provider_->OnDeviceChanged(cast_service_type, true, cast_service);

  BrowserInfo exp_browser_info;
  exp_browser_info.socket_name = "9222";
  exp_browser_info.display_name = cast_display_name;  // From metadata::fn
  exp_browser_info.type = BrowserInfo::kTypeChrome;

  DeviceInfo expected;
  expected.model = cast_service_model;  // From metadata::md
  expected.connected = true;
  expected.browser_info.push_back(exp_browser_info);

  bool was_run = false;
  // Callback should be run, and the queried service should match the expected.
  device_provider_->QueryDeviceInfo(
      cast_service.address.host(),
      base::Bind(&CompareDeviceInfo, &was_run, expected));
  ASSERT_TRUE(was_run);
  was_run = false;

  // Create a non-cast service.
  const std::string other_service_display_name = "OtherDevice";
  const std::string other_service_type = "_other._tcp.local";
  const std::string other_service_model = "Some Other Device";

  ServiceDescription other_service;
  other_service.service_name =
      other_service_display_name + "." + other_service_type;
  other_service.address = net::HostPortPair("10.64.1.101", 1234);
  other_service.metadata.push_back("id=0123456789abcdef0123456789abcdef");
  other_service.metadata.push_back("ve=00");
  other_service.metadata.push_back("md=" + other_service_model);
  ASSERT_TRUE(other_service.ip_address.AssignFromIPLiteral("10.64.1.101"));

  // Callback should not be run, since this service is not yet discovered.
  device_provider_->QueryDeviceInfo(other_service.address.host(),
                                    base::Bind(&DummyCallback, &was_run));
  ASSERT_FALSE(was_run);

  device_provider_->OnDeviceChanged(cast_service_type, true, other_service);

  // Callback should not be run, since non-cast services are not discovered by
  // this device provider.
  device_provider_->QueryDeviceInfo(other_service.address.host(),
                                    base::Bind(&DummyCallback, &was_run));
  ASSERT_FALSE(was_run);

  // Remove the cast service.
  device_provider_->OnDeviceRemoved(cast_service_type,
                                    cast_service.service_name);

  // Callback should not be run, since the cast service has been removed.
  device_provider_->QueryDeviceInfo(cast_service.address.host(),
                                    base::Bind(&DummyCallback, &was_run));
  ASSERT_FALSE(was_run);
}

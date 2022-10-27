// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

namespace {

// Asserts equality between the two objects.
void ExpectEqual(const DialDeviceData& first, const DialDeviceData& second) {
  EXPECT_EQ(first.device_id(), second.device_id());
  EXPECT_EQ(first.label(), second.label());
  EXPECT_EQ(first.device_description_url(), second.device_description_url());
  EXPECT_EQ(first.response_time(), second.response_time());
  EXPECT_EQ(first.max_age(), second.max_age());
  EXPECT_EQ(first.config_id(), second.config_id());
  EXPECT_EQ(first.ip_address(), second.ip_address());
}

bool IsValidUrl(const GURL& url, const net::IPAddress& address) {
  DialDeviceData device_data;
  device_data.set_ip_address(address);
  return device_data.IsValidUrl(url);
}

}  // namespace

TEST(DialDeviceDataTest, TestUpdateFrom) {
  DialDeviceData original;
  original.set_device_id("device_a");
  original.set_label("label_a");
  original.set_device_description_url(GURL("http://127.0.0.1/dd-a.xml"));
  original.set_response_time(base::Time::FromInternalValue(1000));
  original.set_max_age(100);
  original.set_ip_address(net::IPAddress::IPv4Localhost());
  original.set_config_id(1);

  DialDeviceData new_data;
  new_data.set_device_id("device_a");
  new_data.set_device_description_url(GURL("http://127.0.0.1/dd-a.xml"));
  new_data.set_response_time(base::Time::FromInternalValue(1000));
  new_data.set_max_age(100);
  new_data.set_ip_address(net::IPAddress::IPv4Localhost());
  new_data.set_config_id(1);

  DialDeviceData original_1(original);
  EXPECT_FALSE(original_1.UpdateFrom(new_data));
  ExpectEqual(original_1, original);

  DialDeviceData original_2(original);
  new_data.set_max_age(200);
  new_data.set_response_time(base::Time::FromInternalValue(2000));
  EXPECT_TRUE(original_2.UpdateFrom(new_data));

  EXPECT_EQ("device_a", original_2.device_id());
  EXPECT_EQ(GURL("http://127.0.0.1/dd-a.xml"),
            original_2.device_description_url());
  EXPECT_EQ("label_a", original_2.label());
  EXPECT_EQ(200, original_2.max_age());
  EXPECT_EQ(base::Time::FromInternalValue(2000), original_2.response_time());
  EXPECT_EQ(net::IPAddress::IPv4Localhost(), original_2.ip_address());
  EXPECT_EQ(1, original_2.config_id());

  DialDeviceData original_3(original);
  new_data.set_device_description_url(GURL("http://127.0.0.2/dd-b.xml"));
  new_data.set_config_id(2);
  EXPECT_TRUE(original_3.UpdateFrom(new_data));

  EXPECT_EQ("device_a", original_3.device_id());
  EXPECT_EQ(GURL("http://127.0.0.2/dd-b.xml"),
            original_3.device_description_url());
  EXPECT_EQ("label_a", original_3.label());
  EXPECT_EQ(200, original_3.max_age());
  EXPECT_EQ(base::Time::FromInternalValue(2000), original_3.response_time());
  EXPECT_EQ(net::IPAddress::IPv4Localhost(), original_3.ip_address());
  EXPECT_EQ(2, original_3.config_id());

  DialDeviceData original_4(original);
  net::IPAddress new_address;
  CHECK(new_address.AssignFromIPLiteral("127.0.0.2"));
  new_data.set_ip_address(new_address);
  EXPECT_TRUE(original_4.UpdateFrom(new_data));
  EXPECT_EQ(new_address, original_4.ip_address());
}

TEST(DialDeviceDataTest, TestIsValidUrl) {
  net::IPAddress ipv4_address_1;
  ASSERT_TRUE(ipv4_address_1.AssignFromIPLiteral("192.168.1.1"));
  net::IPAddress ipv4_address_2;
  ASSERT_TRUE(ipv4_address_2.AssignFromIPLiteral("192.168.1.2"));
  net::IPAddress ipv4_address_public;
  ASSERT_TRUE(ipv4_address_public.AssignFromIPLiteral("8.8.8.8"));
  net::IPAddress ipv4_address_bad_octets;
  ASSERT_FALSE(ipv4_address_bad_octets.AssignFromIPLiteral("400.500.12.999"));
  net::IPAddress ipv6_address;
  ASSERT_TRUE(ipv6_address.AssignFromIPLiteral(
      "4401:fa00:480:8600:c4ee:4adc:6dd2:8e78"));

  EXPECT_TRUE(IsValidUrl(GURL("http://192.168.1.1:1234/apps"), ipv4_address_1));
  EXPECT_TRUE(
      IsValidUrl(GURL("https://192.168.1.2:4321/apps"), ipv4_address_2));
  EXPECT_TRUE(IsValidUrl(GURL("http://0xC0A80102:1234/apps"), ipv4_address_2));
  EXPECT_TRUE(IsValidUrl(
      GURL("http://[4401:fa00:480:8600:c4ee:4adc:6dd2:8e78]:1234/apps"),
      ipv6_address));

  // URL host does not match address.
  EXPECT_FALSE(
      IsValidUrl(GURL("http://192.168.0.1:1234/apps"), ipv4_address_1));
  EXPECT_FALSE(IsValidUrl(GURL("http://400.500.12.999:1234/apps"),
                          ipv4_address_bad_octets));
  EXPECT_FALSE(
      IsValidUrl(GURL("http://192.168.0.2.com:1234/apps"), ipv4_address_1));

  // URL is publicly routable.
  EXPECT_FALSE(
      IsValidUrl(GURL("http://8.8.8.8:1234/apps"), ipv4_address_public));

  // URL is has a bad scheme.
  EXPECT_FALSE(
      IsValidUrl(GURL("file://192.168.1.1:1234/apps"), ipv4_address_1));

  // URL host is not an IP literal.
  EXPECT_FALSE(
      IsValidUrl(GURL("http://example.com:1234/apps"), ipv4_address_1));
}

}  // namespace media_router

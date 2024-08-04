// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client_mac_util.h"

#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using local_discovery::ExtractServiceInfo;

TEST(ServiceDiscoveryClientMacUtilTest, TestValidInputs) {
  std::optional<local_discovery::ServiceInfo> service_info =
      ExtractServiceInfo("_testing._tcp.local", /*is_service_name=*/false);
  EXPECT_TRUE(service_info);
  EXPECT_FALSE(service_info->instance);
  EXPECT_EQ(service_info->service_type, "_testing._tcp.");
  EXPECT_EQ(service_info->domain, "local.");

  service_info = ExtractServiceInfo("name._testing._tcp.local",
                                    /*is_service_name=*/true);
  EXPECT_TRUE(service_info);
  EXPECT_EQ(service_info->instance.value(), "name");
  EXPECT_EQ(service_info->service_type, "_testing._tcp.");
  EXPECT_EQ(service_info->domain, "local.");

  service_info = ExtractServiceInfo("_printer._sub._testing._tcp.mynetwork",
                                    /*is_service_name=*/false);
  EXPECT_TRUE(service_info);
  EXPECT_FALSE(service_info->instance);
  EXPECT_EQ(service_info->sub_type.value_or(""), "_printer.");
  EXPECT_EQ(service_info->service_type, "_testing._tcp.");
  EXPECT_EQ(service_info->domain, "mynetwork.");

  service_info =
      ExtractServiceInfo("name._printer._sub._testing._tcp.mynetwork",
                         /*is_service_name=*/true);
  EXPECT_TRUE(service_info);
  EXPECT_EQ(service_info->instance.value(), "name");
  EXPECT_EQ(service_info->sub_type.value_or(""), "_printer.");
  EXPECT_EQ(service_info->service_type, "_testing._tcp.");
  EXPECT_EQ(service_info->domain, "mynetwork.");
}

TEST(ServiceDiscoveryClientMacUtilTest, TestInvalidInputs) {
  EXPECT_FALSE(ExtractServiceInfo("", /*is_service_name=*/false));
  EXPECT_FALSE(ExtractServiceInfo(".local", /*is_service_name=*/false));
  EXPECT_FALSE(ExtractServiceInfo("_testing.local", /*is_service_name=*/false));
  EXPECT_FALSE(ExtractServiceInfo("_sub._testing._tcp.local",
                                  /*is_service_name=*/false));

  EXPECT_FALSE(
      ExtractServiceInfo("_testing._tcp.local", /*is_service_name=*/true));
  EXPECT_FALSE(
      ExtractServiceInfo("name._testing.local", /*is_service_name=*/true));
  EXPECT_FALSE(ExtractServiceInfo("name._sub._testing._tcp.local",
                                  /*is_service_name=*/true));
  EXPECT_FALSE(ExtractServiceInfo("Test\x9F\xF0\x92\xA9._testing._tcp.local",
                                  /*is_service_name=*/true));
}

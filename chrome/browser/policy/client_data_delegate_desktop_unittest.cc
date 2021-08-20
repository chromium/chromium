// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/client_data_delegate_desktop.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/features.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(ClientDataDelegateDesktopTest,
     FillRegisterBrowserRequest_BrowserDeviceIdentifier) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kUploadBrowserDeviceIdentifier);

  ClientDataDelegateDesktop client_data_delegate;
  enterprise_management::RegisterBrowserRequest request;
  client_data_delegate.FillRegisterBrowserRequest(&request);

  EXPECT_EQ(request.machine_name(), GetMachineName());
  std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>
      expected_browser_device_identifier = GetBrowserDeviceIdentifier();
  EXPECT_EQ(request.browser_device_identifier().computer_name(),
            expected_browser_device_identifier->computer_name());
  EXPECT_EQ(request.browser_device_identifier().serial_number(),
            expected_browser_device_identifier->serial_number());
}

TEST(ClientDataDelegateDesktopTest,
     FillRegisterBrowserRequest_NoBrowserDeviceIdentifier) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kUploadBrowserDeviceIdentifier);

  ClientDataDelegateDesktop client_data_delegate;
  enterprise_management::RegisterBrowserRequest request;
  client_data_delegate.FillRegisterBrowserRequest(&request);

  EXPECT_EQ(request.machine_name(), GetMachineName());
  EXPECT_TRUE(request.browser_device_identifier().computer_name().empty());
  EXPECT_TRUE(request.browser_device_identifier().serial_number().empty());
}

}  // namespace policy

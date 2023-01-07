// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/client_data_delegate_android.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/system/sys_info.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(ClientDataDelegateAndroidTest, FillRegisterBrowserRequest) {
  base::test::TaskEnvironment task_environment;
  ClientDataDelegateAndroid client_data_delegate;
  enterprise_management::RegisterBrowserRequest request;
  client_data_delegate.FillRegisterBrowserRequest(&request, base::DoNothing());
  task_environment.RunUntilIdle();

  base::SysInfo::HardwareInfo hardware_info;
  base::SysInfo::GetHardwareInfo(base::BindOnce(
      [](base::SysInfo::HardwareInfo* target_info,
         base::SysInfo::HardwareInfo info) { *target_info = std::move(info); },
      &hardware_info));
  task_environment.RunUntilIdle();

  EXPECT_FALSE(request.device_model().empty());
  EXPECT_EQ(request.device_model(), hardware_info.model);
  EXPECT_FALSE(request.brand_name().empty());
  EXPECT_EQ(request.brand_name(), hardware_info.manufacturer);

  // Fields that shouldn't be filled on Android due to Privacy concerns.
  EXPECT_TRUE(request.machine_name().empty());
  EXPECT_TRUE(request.browser_device_identifier().computer_name().empty());
  EXPECT_TRUE(request.browser_device_identifier().serial_number().empty());
}

}  // namespace policy

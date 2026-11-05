// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/client_data_delegate_android.h"

#include "base/android/device_info.h"
#include "base/system/sys_info.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
constexpr char kCustomDeviceName[] = "Custom Pixel Device";
}  // namespace

class ClientDataDelegateAndroidTest : public base::test::WithFeatureOverride,
                                      public testing::Test {
 public:
  ClientDataDelegateAndroidTest()
      : base::test::WithFeatureOverride(kReportAndroidMachineName) {}

 protected:
  void SetUp() override {
    base::android::device_info::set_device_name_for_testing(kCustomDeviceName);
  }

  void TearDown() override {
    base::android::device_info::reset_device_name_for_testing();
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_P(ClientDataDelegateAndroidTest, FillRegisterBrowserRequest) {
  ClientDataDelegateAndroid client_data_delegate;
  enterprise_management::RegisterBrowserRequest request;
  base::test::TestFuture<void> register_done;
  client_data_delegate.FillRegisterBrowserRequest(&request,
                                                  register_done.GetCallback());
  ASSERT_TRUE(register_done.Wait());

  base::test::TestFuture<base::SysInfo::HardwareInfo> hardware_info_future;
  base::SysInfo::GetHardwareInfo(hardware_info_future.GetCallback());
  const base::SysInfo::HardwareInfo& hardware_info = hardware_info_future.Get();

  EXPECT_FALSE(request.device_model().empty());
  EXPECT_EQ(request.device_model(), hardware_info.model);
  EXPECT_FALSE(request.brand_name().empty());
  EXPECT_EQ(request.brand_name(), hardware_info.manufacturer);
  EXPECT_EQ(request.browser_device_identifier().computer_name(),
            IsParamFeatureEnabled() ? kCustomDeviceName : "");

  // Deprecated `machine_name` and `serial_number` fields should not be filled
  // on Android.
  EXPECT_TRUE(request.machine_name().empty());
  EXPECT_TRUE(request.browser_device_identifier().serial_number().empty());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(ClientDataDelegateAndroidTest);

}  // namespace policy

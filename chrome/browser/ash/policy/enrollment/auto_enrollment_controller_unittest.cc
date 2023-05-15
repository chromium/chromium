// // Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_controller.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_install_attributes_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class EnrollmentFwmpHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    ash::InstallAttributesClient::InitializeFake();
    install_attributes_client_ = ash::FakeInstallAttributesClient::Get();
  }

  void TearDown() override { ash::InstallAttributesClient::Shutdown(); }

 protected:
  raw_ptr<ash::FakeInstallAttributesClient, ExperimentalAsh>
      install_attributes_client_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(EnrollmentFwmpHelperTest, NoAvailability) {
  // Fake unavailability of install attributes.
  install_attributes_client_->SetServiceIsAvailable(false);
  install_attributes_client_->ReportServiceIsNotAvailable();

  // Verify that EnrollmentFwmpHelper yields `false`.
  EnrollmentFwmpHelper helper(install_attributes_client_);
  base::test::TestFuture<bool> future_result_holder;
  helper.DetermineDevDisableBoot(future_result_holder.GetCallback());
  EXPECT_FALSE(future_result_holder.Get());
}

TEST_F(EnrollmentFwmpHelperTest, NoFwmpParameters) {
  // Fake FWMP starts out unset
  EnrollmentFwmpHelper helper(install_attributes_client_);
  base::test::TestFuture<bool> future_result_holder;
  helper.DetermineDevDisableBoot(future_result_holder.GetCallback());
  EXPECT_FALSE(future_result_holder.Get());
}

TEST_F(EnrollmentFwmpHelperTest, NoDevDisableBoot) {
  // Fake FWMP.dev_disable_boot == 0.
  user_data_auth::SetFirmwareManagementParametersRequest request;
  request.mutable_fwmp()->set_flags(0u);
  base::test::TestFuture<
      absl::optional<user_data_auth::SetFirmwareManagementParametersReply>>
      future_fwmp;
  install_attributes_client_->SetFirmwareManagementParameters(
      request, future_fwmp.GetCallback());
  ASSERT_TRUE(future_fwmp.Get());

  // Verify that EnrollmentFwmpHelper yields `false`.
  EnrollmentFwmpHelper helper(install_attributes_client_);
  base::test::TestFuture<bool> future_result_holder;
  helper.DetermineDevDisableBoot(future_result_holder.GetCallback());
  EXPECT_FALSE(future_result_holder.Get());
}

TEST_F(EnrollmentFwmpHelperTest, DevDisableBoot) {
  // Fake FWMP.dev_disable_boot == 1.
  user_data_auth::SetFirmwareManagementParametersRequest request;
  request.mutable_fwmp()->set_flags(cryptohome::DEVELOPER_DISABLE_BOOT);
  base::test::TestFuture<
      absl::optional<user_data_auth::SetFirmwareManagementParametersReply>>
      future_fwmp;
  install_attributes_client_->SetFirmwareManagementParameters(
      request, future_fwmp.GetCallback());
  ASSERT_TRUE(future_fwmp.Get());

  // Verify that EnrollmentFwmpHelper yields `true`.
  EnrollmentFwmpHelper helper(install_attributes_client_);
  base::test::TestFuture<bool> future_result;
  helper.DetermineDevDisableBoot(future_result.GetCallback());
  EXPECT_TRUE(future_result.Get());
}

}  // namespace policy

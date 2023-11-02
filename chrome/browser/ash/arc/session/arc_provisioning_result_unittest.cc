// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"

#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

TEST(ArcProvisioningResultTest, SignInError) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_FALSE(result1.sign_in_error());

  ArcProvisioningResult result2(arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewGeneralError(
          arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR)));
  EXPECT_TRUE(result2.sign_in_error());
}

TEST(ArcProvisioningResultTest, GmsSignInError) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_FALSE(result1.gms_sign_in_error());

  ArcProvisioningResult result2(arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewGeneralError(
          arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR)));
  EXPECT_FALSE(result2.gms_sign_in_error());

  ArcProvisioningResult result3(arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewSignInError(
          arc::mojom::GMSSignInError::GMS_SIGN_IN_TIMEOUT)));

  EXPECT_EQ(result3.gms_sign_in_error(),
            arc::mojom::GMSSignInError::GMS_SIGN_IN_TIMEOUT);
}

TEST(ArcProvisioningResultTest, GmsCheckInError) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_FALSE(result1.gms_check_in_error());

  ArcProvisioningResult result2(arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewCheckInError(
          arc::mojom::GMSCheckInError::GMS_CHECK_IN_INTERNAL_ERROR)));

  EXPECT_EQ(result2.gms_check_in_error(),
            arc::mojom::GMSCheckInError::GMS_CHECK_IN_INTERNAL_ERROR);
}

TEST(ArcProvisioningResultTest, CloudProvisionFlowError) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_FALSE(result1.cloud_provision_flow_error());

  ArcProvisioningResult result2(arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewCloudProvisionFlowError(
          arc::mojom::CloudProvisionFlowError::ERROR_ACCOUNT_NOT_ALLOWLISTED)));

  EXPECT_EQ(result2.cloud_provision_flow_error(),
            arc::mojom::CloudProvisionFlowError::ERROR_ACCOUNT_NOT_ALLOWLISTED);
}

TEST(ArcProvisioningResultTest, Success) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_FALSE(result1.is_success());

  ArcProvisioningResult result2(arc::mojom::ArcSignInResult::NewSuccess(
      arc::mojom::ArcSignInSuccess::SUCCESS));
  EXPECT_TRUE(result2.is_success());

  ArcProvisioningResult result3(arc::mojom::ArcSignInResult::NewSuccess(
      arc::mojom::ArcSignInSuccess::SUCCESS_ALREADY_PROVISIONED));
  EXPECT_TRUE(result3.is_success());
}

TEST(ArcProvisioningResultTest, StopReason) {
  ArcProvisioningResult result1(arc::mojom::ArcSignInResult::NewSuccess(
      arc::mojom::ArcSignInSuccess::SUCCESS));
  EXPECT_FALSE(result1.stop_reason());

  ArcProvisioningResult result2(ArcStopReason::CRASH);
  EXPECT_EQ(ArcStopReason::CRASH, result2.stop_reason());

  ArcProvisioningResult result3(ArcStopReason::SHUTDOWN);
  EXPECT_EQ(ArcStopReason::SHUTDOWN, result3.stop_reason());

  ArcProvisioningResult result4(ArcStopReason::GENERIC_BOOT_FAILURE);
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE, result4.stop_reason());

  ArcProvisioningResult result5(ArcStopReason::LOW_DISK_SPACE);
  EXPECT_EQ(ArcStopReason::LOW_DISK_SPACE, result5.stop_reason());
}

TEST(ArcProvisioningResultTest, Timedout) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_FALSE(result1.is_timedout());

  ArcProvisioningResult result2(ChromeProvisioningTimeout{});
  EXPECT_TRUE(result2.is_timedout());
}

TEST(ArcProvisioningResultTest, GeneralError) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_FALSE(result1.general_error());

  ArcProvisioningResult result2(arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewGeneralError(
          arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR)));

  EXPECT_EQ(result2.general_error(),
            arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR);
}

}  // namespace arc

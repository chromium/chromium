// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/session/arc_provisioning_result.h"

#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

TEST(ArcProvisioningResultTest, HasSignInResult) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_FALSE(result1.has_sign_in_result());

  ArcProvisioningResult result2(arc::mojom::ArcSignInResult::NewSuccess(
      arc::mojom::ArcSignInSuccess::SUCCESS));
  EXPECT_TRUE(result2.has_sign_in_result());
}

TEST(ArcProvisioningResultTest, HasSignInError) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_FALSE(result1.has_sign_in_error());

  ArcProvisioningResult result2(arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewGeneralError(
          arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR)));
  EXPECT_TRUE(result2.has_sign_in_error());
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

TEST(ArcProvisioningResultTest, Stopped) {
  ArcProvisioningResult result1(arc::mojom::ArcSignInResult::NewSuccess(
      arc::mojom::ArcSignInSuccess::SUCCESS));
  EXPECT_FALSE(result1.is_stopped());

  ArcProvisioningResult result2(ArcStopReason::CRASH);
  EXPECT_TRUE(result2.is_stopped());

  ArcProvisioningResult result3(ArcStopReason::SHUTDOWN);
  EXPECT_TRUE(result3.is_stopped());

  ArcProvisioningResult result4(ArcStopReason::GENERIC_BOOT_FAILURE);
  EXPECT_TRUE(result4.is_stopped());

  ArcProvisioningResult result5(ArcStopReason::LOW_DISK_SPACE);
  EXPECT_TRUE(result5.is_stopped());
}

TEST(ArcProvisioningResultTest, Timedout) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_FALSE(result1.is_timedout());

  ArcProvisioningResult result2(ChromeProvisioningTimeout{});
  EXPECT_TRUE(result2.is_timedout());
}

TEST(ArcProvisioningResultTest, HasGeneralError) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_FALSE(result1.has_general_error(
      arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR));

  ArcProvisioningResult result2(arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewGeneralError(
          arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR)));
  EXPECT_TRUE(result2.has_general_error(
      arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR));
}

TEST(ArcProvisioningResultTest, StopReason) {
  ArcProvisioningResult result1(ArcStopReason::CRASH);
  EXPECT_EQ(ArcStopReason::CRASH, result1.stop_reason());

  ArcProvisioningResult result2(ArcStopReason::SHUTDOWN);
  EXPECT_EQ(ArcStopReason::SHUTDOWN, result2.stop_reason());

  ArcProvisioningResult result3(ArcStopReason::GENERIC_BOOT_FAILURE);
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE, result3.stop_reason());

  ArcProvisioningResult result4(ArcStopReason::LOW_DISK_SPACE);
  EXPECT_EQ(ArcStopReason::LOW_DISK_SPACE, result4.stop_reason());
}

}  // namespace arc

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/monthly_use_case_impl.h"

#include "ash/components/device_activity/device_activity_controller.h"
#include "ash/components/device_activity/fresnel_pref_names.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash {
namespace device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// Initialize fake values used by the |MonthlyUseCaseImpl|.
constexpr char kFakePsmDeviceActiveSecret[] = "FAKE_PSM_DEVICE_ACTIVE_SECRET";

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

}  // namespace

// TODO(hirthanan): Move shared tests to DeviceActiveUseCase base class.
class MonthlyUseCaseImplTest : public testing::Test {
 public:
  MonthlyUseCaseImplTest() = default;
  MonthlyUseCaseImplTest(const MonthlyUseCaseImplTest&) = delete;
  MonthlyUseCaseImplTest& operator=(const MonthlyUseCaseImplTest&) = delete;
  ~MonthlyUseCaseImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    DeviceActivityController::RegisterPrefs(local_state_.registry());

    monthly_use_case_impl_ = std::make_unique<MonthlyUseCaseImpl>(
        kFakePsmDeviceActiveSecret, kFakeChromeParameters, &local_state_);
  }

  void TearDown() override { monthly_use_case_impl_.reset(); }

  std::unique_ptr<MonthlyUseCaseImpl> monthly_use_case_impl_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
};

TEST_F(MonthlyUseCaseImplTest, CheckIfLastKnownPingTimestampNotSet) {
  EXPECT_FALSE(monthly_use_case_impl_->IsLastKnownPingTimestampSet());
}

TEST_F(MonthlyUseCaseImplTest, CheckIfLastKnownPingTimestampSet) {
  // Create fixed timestamp to see if local state updates value correctly.
  base::Time new_monthly_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_monthly_ts));

  // Update local state with fixed timestamp.
  monthly_use_case_impl_->SetLastKnownPingTimestamp(new_monthly_ts);

  EXPECT_EQ(monthly_use_case_impl_->GetLastKnownPingTimestamp(),
            new_monthly_ts);
  EXPECT_TRUE(monthly_use_case_impl_->IsLastKnownPingTimestampSet());
}

TEST_F(MonthlyUseCaseImplTest, CheckGenerateUTCWindowIdentifierHasValidFormat) {
  // Create fixed timestamp used to generate a fixed window identifier.
  base::Time new_monthly_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_monthly_ts));

  std::string window_id =
      monthly_use_case_impl_->GenerateUTCWindowIdentifier(new_monthly_ts);

  EXPECT_EQ(window_id.size(), 6);
  EXPECT_EQ(window_id, "202201");
}

TEST_F(MonthlyUseCaseImplTest, CheckPsmIdEmptyIfWindowIdIsNotSet) {
  // |monthly_use_case_impl_| must set the window id before generating the psm
  // id.
  EXPECT_THAT(monthly_use_case_impl_->GetPsmIdentifier(),
              testing::Eq(absl::nullopt));
}

TEST_F(MonthlyUseCaseImplTest, CheckPsmIdGeneratedCorrectly) {
  // Create fixed timestamp used to generate a fixed window identifier.
  // The window id must be set before generating the psm id.
  base::Time new_monthly_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_monthly_ts));

  std::string window_id =
      monthly_use_case_impl_->GenerateUTCWindowIdentifier(new_monthly_ts);
  monthly_use_case_impl_->SetWindowIdentifier(window_id);

  absl::optional<psm_rlwe::RlwePlaintextId> psm_id =
      monthly_use_case_impl_->GetPsmIdentifier();

  EXPECT_TRUE(psm_id.has_value());

  // Verify the PSM value is correct for parameters supplied by the unit tests.
  std::string unhashed_psm_id = base::JoinString(
      {psm_rlwe::RlweUseCase_Name(monthly_use_case_impl_->GetPsmUseCase()),
       window_id},
      "|");
  std::string expected_psm_id_hex = monthly_use_case_impl_->GetDigestString(
      kFakePsmDeviceActiveSecret, unhashed_psm_id);
  EXPECT_EQ(psm_id.value().sensitive_id(), expected_psm_id_hex);
}

TEST_F(MonthlyUseCaseImplTest, PingRequiredInNonOverlappingUTCWindows) {
  base::Time last_monthly_ts;
  base::Time current_monthly_ts;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 00:00:00 GMT", &last_monthly_ts));
  monthly_use_case_impl_->SetLastKnownPingTimestamp(last_monthly_ts);

  EXPECT_TRUE(
      base::Time::FromString("25 Feb 2022 00:00:00 GMT", &current_monthly_ts));

  EXPECT_TRUE(monthly_use_case_impl_->IsDevicePingRequired(current_monthly_ts));
}

TEST_F(MonthlyUseCaseImplTest, PingNotRequiredInOverlappingUTCWindows) {
  base::Time last_monthly_ts;
  base::Time current_monthly_ts;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 12:59:59 GMT", &last_monthly_ts));
  monthly_use_case_impl_->SetLastKnownPingTimestamp(last_monthly_ts);

  EXPECT_TRUE(
      base::Time::FromString("25 Jan 2022 15:59:59 GMT", &current_monthly_ts));

  EXPECT_FALSE(
      monthly_use_case_impl_->IsDevicePingRequired(current_monthly_ts));
}

TEST_F(MonthlyUseCaseImplTest, CheckIfPingRequiredInUTCBoundaryCases) {
  base::Time last_monthly_ts;
  base::Time current_monthly_ts;

  EXPECT_TRUE(
      base::Time::FromString("31 Jan 2022 23:59:59 GMT", &last_monthly_ts));
  monthly_use_case_impl_->SetLastKnownPingTimestamp(last_monthly_ts);

  EXPECT_TRUE(
      base::Time::FromString("01 Feb 2022 00:00:00 GMT", &current_monthly_ts));

  EXPECT_TRUE(monthly_use_case_impl_->IsDevicePingRequired(current_monthly_ts));

  // Set last_monthly_ts as a date after current_monthly_ts.
  EXPECT_TRUE(
      base::Time::FromString("01 Feb 2022 00:00:00 GMT", &last_monthly_ts));
  monthly_use_case_impl_->SetLastKnownPingTimestamp(last_monthly_ts);

  EXPECT_TRUE(
      base::Time::FromString("31 Jan 2022 23:59:59 GMT", &current_monthly_ts));

  // Since the current_monthly_ts is prior to the last_monthly_ts, the function
  // should return false.
  EXPECT_FALSE(
      monthly_use_case_impl_->IsDevicePingRequired(current_monthly_ts));
}

TEST_F(MonthlyUseCaseImplTest, SameMonthTimestampsHaveSameWindowId) {
  base::Time monthly_ts_1;
  base::Time monthly_ts_2;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 00:00:00 GMT", &monthly_ts_1));
  EXPECT_TRUE(
      base::Time::FromString("31 Jan 2022 23:59:59 GMT", &monthly_ts_2));

  EXPECT_EQ(monthly_use_case_impl_->GenerateUTCWindowIdentifier(monthly_ts_1),
            monthly_use_case_impl_->GenerateUTCWindowIdentifier(monthly_ts_2));
}

TEST_F(MonthlyUseCaseImplTest, DifferentWindowIdGeneratesDifferentPsmId) {
  base::Time monthly_ts_1;
  base::Time monthly_ts_2;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 00:00:00 GMT", &monthly_ts_1));
  EXPECT_TRUE(
      base::Time::FromString("01 Feb 2022 00:00:00 GMT", &monthly_ts_2));

  std::string window_id_1 =
      monthly_use_case_impl_->GenerateUTCWindowIdentifier(monthly_ts_1);
  std::string window_id_2 =
      monthly_use_case_impl_->GenerateUTCWindowIdentifier(monthly_ts_2);

  monthly_use_case_impl_->SetWindowIdentifier(window_id_1);
  absl::optional<psm_rlwe::RlwePlaintextId> psm_id_1 =
      monthly_use_case_impl_->GetPsmIdentifier();

  monthly_use_case_impl_->SetWindowIdentifier(window_id_2);
  absl::optional<psm_rlwe::RlwePlaintextId> psm_id_2 =
      monthly_use_case_impl_->GetPsmIdentifier();

  EXPECT_NE(psm_id_1.value().sensitive_id(), psm_id_2.value().sensitive_id());
}

}  // namespace device_activity
}  // namespace ash

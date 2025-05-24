// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"

#include <string>
#include <tuple>

#include "ash/constants/ash_switches.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_test_helper.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "net/base/load_flags.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using USDStatus = policy::AutoEnrollmentTypeChecker::USDStatus;

}  // namespace

namespace policy {

class AutoEnrollmentTypeCheckerTest : public testing::Test {
 public:
  AutoEnrollmentTypeCheckerTest() = default;
  ~AutoEnrollmentTypeCheckerTest() override = default;

 protected:
  void SetUpFlexDeviceWithFREOnFlexEnabled() {
    enrollment_test_helper_.SetUpFlexDevice();
    enrollment_test_helper_.EnableFREOnFlex();
  }

  void SetUpFlexDeviceWithFREOnFlexDisabled() {
    enrollment_test_helper_.SetUpFlexDevice();
    enrollment_test_helper_.DisableFREOnFlex();
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr bool is_google_branded_ = true;
#else
  static constexpr bool is_google_branded_ = false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  base::test::ScopedCommandLine command_line_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  test::EnrollmentTestHelper enrollment_test_helper_{
      &command_line_, &fake_statistics_provider_};
  base::HistogramTester histograms_;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(AutoEnrollmentTypeCheckerTest, Default) {
  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kEnabledOnOfficialGoogleChrome, 1);
}

TEST_F(AutoEnrollmentTypeCheckerTest, FlexDevice) {
  enrollment_test_helper_.SetUpFlexDevice();

  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kEnabledOnOfficialGoogleFlex, 1);
}

TEST_F(AutoEnrollmentTypeCheckerTest, AlwaysSwitch) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::kUnifiedStateDeterminationAlways);

  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kEnabledViaAlwaysSwitch, 1);
}

TEST_F(AutoEnrollmentTypeCheckerTest, NeverSwitch) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);

  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kDisabledViaNeverSwitch, 1);
}

TEST_F(AutoEnrollmentTypeCheckerTest, NonChrome) {
  enrollment_test_helper_.SetUpNonchromeDevice();

  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kDisabledOnNonChromeDevice, 1);
}
#else
TEST_F(AutoEnrollmentTypeCheckerTest, UnbrandedBuild) {
  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kDisabledOnUnbrandedBuild, 1);
}
#endif

// An enum for the kind of Chromium OS running on the device.
enum class DeviceOs {
  Chrome = 0,
  Nonchrome = 1,
  // TODO(b/331677599): Delete FlexWithoutFRE, and make FlexWithFRE just Flex.
  FlexWithoutFRE = 2,
  FlexWithFRE = 3,
};

// This is parameterized by device OS.
class AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP
    : public AutoEnrollmentTypeCheckerTest,
      public testing::WithParamInterface<std::tuple<DeviceOs>> {
 protected:
  void SetUp() override {
    AutoEnrollmentTypeCheckerTest::SetUp();
    // TODO(b/353731379): Remove when removing legacy state determination code.
    command_line_.GetProcessCommandLine()->RemoveSwitch(
        ash::switches::kEnterpriseEnableUnifiedStateDetermination);
    if (device_os_ == DeviceOs::Nonchrome) {
      enrollment_test_helper_.SetUpNonchromeDevice();
    } else if (device_os_ == DeviceOs::FlexWithoutFRE) {
      SetUpFlexDeviceWithFREOnFlexDisabled();
    } else if (device_os_ == DeviceOs::FlexWithFRE) {
      SetUpFlexDeviceWithFREOnFlexEnabled();
    }
  }

  bool IsFRESupportedByDevice() {
    return google_branded_ && (device_os_ == DeviceOs::Chrome ||
                               device_os_ == DeviceOs::FlexWithFRE);
  }

  bool IsOfficialGoogleOS() {
    return google_branded_ && (device_os_ == DeviceOs::Chrome ||
                               device_os_ == DeviceOs::FlexWithoutFRE ||
                               device_os_ == DeviceOs::FlexWithFRE);
  }

  const DeviceOs device_os_ = std::get<0>(GetParam());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const bool google_branded_ = true;
#else
  const bool google_branded_ = false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

TEST_P(AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP, Default) {
  EXPECT_EQ(AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled(),
            IsOfficialGoogleOS());
}

TEST_P(AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP, OfficialBuild) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::kUnifiedStateDeterminationOfficialBuild);

  EXPECT_EQ(AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled(),
            IsOfficialGoogleOS());
}

TEST_P(AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP, Never) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled());
}

TEST_P(AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP, Always) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::kUnifiedStateDeterminationAlways);

  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled());
}

INSTANTIATE_TEST_SUITE_P(
    AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestSuite,
    AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP,
    testing::Values(DeviceOs::Chrome,
                    DeviceOs::Nonchrome,
                    DeviceOs::FlexWithoutFRE,
                    DeviceOs::FlexWithFRE));

}  // namespace policy
